import { DeviceHub } from './DeviceHub';

export { DeviceHub };

interface Env {
  DB: D1Database;
  DEVICE_HUB: DurableObjectNamespace;
}

const CORS = { 'Access-Control-Allow-Origin': '*', 'Content-Type': 'application/json' };

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);

    if (request.method === 'OPTIONS') {
      return new Response(null, { headers: CORS });
    }

    if (url.pathname === '/health') {
      return new Response(JSON.stringify({ status: 'ok' }), { headers: CORS });
    }

    // WebSocket upgrade — DO path
    if (url.pathname === '/ws' && request.headers.get('Upgrade') === 'websocket') {
      const officeId = url.searchParams.get('office_id') || 'default';
      const id = env.DEVICE_HUB.idFromName(`v2-office-${officeId}`);
      return env.DEVICE_HUB.get(id).fetch(request);
    }

    // Relay API — D1 隊列（可靠）
    if (url.pathname === '/relay' && request.method === 'POST') {
      const body = await request.json() as { device_id?: string; relay1?: number; relay2?: number };
      if (!body.device_id) return new Response(JSON.stringify({ error: 'device_id required' }), { status: 400, headers: CORS });

      await env.DB.prepare(
        'INSERT INTO relay_queue (device_id, relay1, relay2, ph_cal) VALUES (?, ?, ?, ?)'
      ).bind(body.device_id, body.relay1 ?? null, body.relay2 ?? null, body.ph_cal ?? null).run();

      await env.DB.prepare(
        'INSERT INTO relay_log (device_id, relay1, relay2, ph_cal, status) VALUES (?, ?, ?, ?, ?)'
      ).bind(body.device_id, body.relay1 ?? null, body.relay2 ?? null, body.ph_cal ?? null, 'sent').run();

      return new Response(JSON.stringify({ ok: true }), { headers: CORS });
    }

    return new Response('Not found', { status: 404, headers: CORS });
  },
};
