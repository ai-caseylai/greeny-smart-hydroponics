export class DeviceHub {
  private state: DurableObjectState;
  private env: Env;
  private devices: Map<string, WebSocket> = new Map();
  private dashboards: Set<WebSocket> = new Set();

  constructor(state: DurableObjectState, env: Env) {
    this.state = state;
    this.env = env;
  }

  async fetch(request: Request): Promise<Response> {
    const url = new URL(request.url);

    if (request.headers.get('Upgrade') !== 'websocket') {
      // REST endpoint: device list
      if (url.pathname === '/devices') {
        return new Response(JSON.stringify({
          devices: Array.from(this.devices.keys()),
          dashboards: this.dashboards.size,
        }), { headers: { 'Content-Type': 'application/json' } });
      }
      // REST endpoint: relay command (直接 HTTP)
      if (url.pathname === '/relay-cmd' && request.method === 'POST') {
        const body = await request.json() as { device_id?: string; relay1?: number; relay2?: number };
        if (body.device_id) {
          const ws = this.devices.get(body.device_id);
          if (ws) {
            ws.send(JSON.stringify({
              type: 'relay_cmd',
              relay1: body.relay1,
              relay2: body.relay2,
            }));
            return new Response(JSON.stringify({ ok: true, device_id: body.device_id }), { headers: { 'Content-Type': 'application/json' } });
          }
          return new Response(JSON.stringify({ error: 'device not connected' }), { status: 404, headers: { 'Content-Type': 'application/json' } });
        }
        return new Response(JSON.stringify({ error: 'device_id required' }), { status: 400, headers: { 'Content-Type': 'application/json' } });
      }
      return new Response('Expected WebSocket upgrade', { status: 400 });
    }

    const isDashboard = url.searchParams.get('dashboard') === '1';
    const deviceId = url.searchParams.get('device_id') || '';

    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair) as [WebSocket, WebSocket];

    if (isDashboard) {
      this.state.acceptWebSocket(server, ['dashboard']);
      this.dashboards.add(server);

      // Send current device list to newly connected dashboard
      server.send(JSON.stringify({
        type: 'init',
        devices: Array.from(this.devices.keys()),
      }));
    } else if (deviceId) {
      this.state.acceptWebSocket(server, [deviceId]);

      // Close existing connection for this device if any
      const existing = this.devices.get(deviceId);
      if (existing) {
        try { existing.close(1000, 'Replaced by new connection'); } catch {}
      }
      this.devices.set(deviceId, server);

      // Mark device online in D1
      const now = Math.floor(Date.now() / 1000);
      this.env.DB.prepare(
        `INSERT INTO devices (id, name, floor, location, status, last_seen)
         VALUES (?, ?, 1, '', 'online', ?)
         ON CONFLICT(id) DO UPDATE SET status = 'online', last_seen = ?`
      ).bind(deviceId, deviceId, now, now).run().catch(() => {});

      // Notify dashboards
      this.broadcastToDashboards({
        type: 'device_status',
        device_id: deviceId,
        status: 'online',
      });
    }

    return new Response(null, { status: 101, webSocket: client });
  }

  async webSocketMessage(ws: WebSocket, msg: string | ArrayBuffer) {
    if (typeof msg !== 'string') return;

    let data: any;
    try { data = JSON.parse(msg); } catch { return; }

    switch (data.type) {
      case 'telemetry': {
        const deviceId = data.device_id;
        if (!deviceId) return;

        const now = Math.floor(Date.now() / 1000);
        const tsMs = data.ts_ms || Date.now();
        const ph = data.ph || 0;
        const tds = data.tds || data.ec || 0;
        const waterTemp = data.water_temp || 0;

        this.env.DB.prepare(
          `INSERT INTO telemetry (device_id, ph, ec, water_temp, water_level, ndvi, spectral_red, spectral_green, spectral_blue, spectral_nir, relay1, relay2, ts_ms, created_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`
        ).bind(
          deviceId,
          ph, tds, waterTemp, 0,
          0, 0, 0, 0, 0,
          data.relay1 || 0, data.relay2 || 0, tsMs, now
        ).run().catch(() => {});

        // Check relay queue for this device
        const relayCmd = await this.env.DB.prepare(
          'SELECT id, relay1, relay2, ph_cal FROM relay_queue WHERE device_id = ? ORDER BY id DESC LIMIT 1'
        ).bind(deviceId).first<{ id: number; relay1: number | null; relay2: number | null; ph_cal: number | null }>();
        if (relayCmd) {
          const cmd: any = { type: 'relay_cmd' };
          if (relayCmd.relay1 !== null) cmd.relay1 = relayCmd.relay1;
          if (relayCmd.relay2 !== null) cmd.relay2 = relayCmd.relay2;
          if (relayCmd.ph_cal !== null) cmd.ph_cal = relayCmd.ph_cal;
          ws.send(JSON.stringify(cmd));
          await this.env.DB.prepare('DELETE FROM relay_queue WHERE device_id = ? AND id <= ?')
            .bind(deviceId, relayCmd.id).run();
        }

        // Update last_seen on every telemetry
        this.env.DB.prepare(
          `UPDATE devices SET last_seen = ?, status = 'online' WHERE id = ?`
        ).bind(now, deviceId).run().catch(() => {});

        if (ph && (ph < 5.5 || ph > 8.5)) {
          this.env.DB.prepare(
            `INSERT INTO alerts (device_id, type, message, severity, created_at) VALUES (?, 'ph_abnormal', ?, 'warning', ?)`
          ).bind(deviceId, `${deviceId} pH異常：${ph}`, now).run().catch(() => {});
        }
        if (tds && tds > 2000) {
          this.env.DB.prepare(
            `INSERT INTO alerts (device_id, type, message, severity, created_at) VALUES (?, 'ec_high', ?, 'warning', ?)`
          ).bind(deviceId, `${deviceId} TDS過高：${tds}`, now).run().catch(() => {});
        }
        if (waterTemp && (waterTemp < 18 || waterTemp > 30)) {
          this.env.DB.prepare(
            `INSERT INTO alerts (device_id, type, message, severity, created_at) VALUES (?, 'temp_abnormal', ?, 'warning', ?)`
          ).bind(deviceId, `${deviceId} 水溫異常：${waterTemp}°C`, now).run().catch(() => {});
        }

        this.broadcastToDashboards({
          type: 'telemetry_update',
          device_id: deviceId,
          data: {
            ph, tds, water_temp: waterTemp,
            relay1: data.relay1, relay2: data.relay2,
          },
          ts_ms: tsMs,
        });
        break;
      }

      case 'relay': {
        const deviceId = data.device_id;
        if (!deviceId) return;
        const targetWs = this.devices.get(deviceId);
        if (targetWs) {
          const cmd: any = { type: 'relay_cmd' };
          for (let i = 1; i <= 4; i++) {
            const key = `relay${i}`;
            if (data[key] !== undefined) cmd[key] = data[key];
          }
          if (data.ph_cal !== undefined) cmd.ph_cal = data.ph_cal;
          targetWs.send(JSON.stringify(cmd));
          this.broadcastToDashboards({
            type: 'relay_ack',
            device_id: deviceId,
            ...cmd,
          });
        }
        break;
      }

      case 'ping': {
        ws.send(JSON.stringify({ type: 'pong' }));
        // Check relay queue on ping too (for faster response)
        const deviceId = [...this.devices.entries()].find(([, s]) => s === ws)?.[0];
        if (deviceId) {
          const relayCmd = await this.env.DB.prepare(
            'SELECT id, relay1, relay2, ph_cal FROM relay_queue WHERE device_id = ? ORDER BY id DESC LIMIT 1'
          ).bind(deviceId).first<{ id: number; relay1: number | null; relay2: number | null; ph_cal: number | null }>();
          if (relayCmd) {
            const cmd: any = { type: 'relay_cmd' };
            if (relayCmd.relay1 !== null) cmd.relay1 = relayCmd.relay1;
            if (relayCmd.relay2 !== null) cmd.relay2 = relayCmd.relay2;
            if (relayCmd.ph_cal !== null) cmd.ph_cal = relayCmd.ph_cal;
            ws.send(JSON.stringify(cmd));
            await this.env.DB.prepare('DELETE FROM relay_queue WHERE device_id = ? AND id <= ?').bind(deviceId, relayCmd.id).run();
          }
        }
        break;
      }
    }
  }

  async webSocketClose(ws: WebSocket, code: number, reason: string) {
    this.dashboards.delete(ws);

    for (const [deviceId, deviceWs] of this.devices) {
      if (deviceWs === ws) {
        this.devices.delete(deviceId);
        this.env.DB.prepare(
          `UPDATE devices SET status = 'offline' WHERE id = ?`
        ).bind(deviceId).run().catch(() => {});

        this.broadcastToDashboards({
          type: 'device_status',
          device_id: deviceId,
          status: 'offline',
        });
        break;
      }
    }
  }

  private broadcastToDashboards(msg: any) {
    const text = JSON.stringify(msg);
    for (const ws of this.dashboards) {
      try { ws.send(text); } catch { this.dashboards.delete(ws); }
    }
  }
}

interface Env {
  DB: D1Database;
  DEVICE_HUB: DurableObjectNamespace;
}
