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
      // REST endpoint for getting connected device list
      if (url.pathname === '/devices') {
        return new Response(JSON.stringify({
          devices: Array.from(this.devices.keys()),
          dashboards: this.dashboards.size,
        }), { headers: { 'Content-Type': 'application/json' } });
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

        // Store in D1
        const now = Math.floor(Date.now() / 1000);
        const tsMs = data.ts_ms || Date.now();
        this.env.DB.prepare(
          `INSERT INTO telemetry (device_id, ph, ec, water_temp, water_level, ndvi, spectral_red, spectral_green, spectral_blue, spectral_nir, relay1, relay2, ts_ms, created_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`
        ).bind(
          deviceId,
          data.ph || 0, data.ec || 0, data.water_temp || 0, data.water_level || 0,
          data.ndvi || 0, data.spectral_red || 0, data.spectral_green || 0,
          data.spectral_blue || 0, data.spectral_nir || 0,
          data.relay1 || 0, data.relay2 || 0, tsMs, now
        ).run().catch(() => {});

        // Check alerts
        if (data.ph && (data.ph < 5.5 || data.ph > 7.0)) {
          this.env.DB.prepare(
            `INSERT INTO alerts (device_id, type, message, severity, created_at) VALUES (?, 'ph_abnormal', ?, 'warning', ?)`
          ).bind(deviceId, `${deviceId} pH異常：${data.ph}`, now).run().catch(() => {});
        }
        if (data.ec && data.ec > 2000) {
          this.env.DB.prepare(
            `INSERT INTO alerts (device_id, type, message, severity, created_at) VALUES (?, 'ec_high', ?, 'warning', ?)`
          ).bind(deviceId, `${deviceId} EC過高：${data.ec}`, now).run().catch(() => {});
        }
        if (data.water_temp && (data.water_temp < 18 || data.water_temp > 30)) {
          this.env.DB.prepare(
            `INSERT INTO alerts (device_id, type, message, severity, created_at) VALUES (?, 'temp_abnormal', ?, 'warning', ?)`
          ).bind(deviceId, `${deviceId} 水溫異常：${data.water_temp}°C`, now).run().catch(() => {});
        }

        // Forward to dashboards
        this.broadcastToDashboards({
          type: 'telemetry_update',
          device_id: deviceId,
          data: {
            ph: data.ph, ec: data.ec, water_temp: data.water_temp,
            water_level: data.water_level, ndvi: data.ndvi,
            spectral_red: data.spectral_red, spectral_green: data.spectral_green,
            spectral_blue: data.spectral_blue, spectral_nir: data.spectral_nir,
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
          targetWs.send(JSON.stringify({
            type: 'relay_cmd',
            relay1: data.relay1,
            relay2: data.relay2,
          }));
          // Acknowledge to dashboard
          this.broadcastToDashboards({
            type: 'relay_ack',
            device_id: deviceId,
            relay1: data.relay1,
            relay2: data.relay2,
          });
        }
        break;
      }

      case 'ping': {
        ws.send(JSON.stringify({ type: 'pong' }));
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
