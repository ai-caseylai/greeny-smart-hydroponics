import { DeviceHub } from './DeviceHub';

export { DeviceHub };

interface Env {
  DB: D1Database;
  DEVICE_HUB: DurableObjectNamespace;
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);

    // CORS preflight
    if (request.method === 'OPTIONS') {
      return new Response(null, {
        headers: {
          'Access-Control-Allow-Origin': '*',
          'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
          'Access-Control-Allow-Headers': 'Content-Type, Authorization',
        },
      });
    }

    // Health check
    if (url.pathname === '/health') {
      return new Response(JSON.stringify({ status: 'ok', service: 'greeny-ws' }), {
        headers: { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*' },
      });
    }

    // WebSocket upgrade
    if (url.pathname === '/ws' && request.headers.get('Upgrade') === 'websocket') {
      const officeId = url.searchParams.get('office_id') || 'default';
      const id = env.DEVICE_HUB.idFromName(`v2-office-${officeId}`);
      const stub = env.DEVICE_HUB.get(id);
      return stub.fetch(request);
    }

    // HTTP relay: 直接送到 DO，不經過 WebSocket
    if (url.pathname === '/relay' && request.method === 'POST') {
      const body = await request.json() as { device_id?: string; relay1?: number; relay2?: number };
      if (!body.device_id) {
        return new Response(JSON.stringify({ error: 'device_id required' }), { status: 400, headers: cors });
      }
      const officeId = url.searchParams.get('office_id') || '9';
      const id = env.DEVICE_HUB.idFromName(`v2-office-${officeId}`);
      const stub = env.DEVICE_HUB.get(id);
      // 透過 HTTP 呼叫 DO 的 relay endpoint
      const doUrl = new URL(request.url);
      doUrl.pathname = '/relay-cmd';
      const doReq = new Request(doUrl.toString(), { method: 'POST', body: JSON.stringify(body), headers: { 'Content-Type': 'application/json' } });
      return stub.fetch(doReq);
    }

    const cors = { 'Access-Control-Allow-Origin': '*', 'Content-Type': 'application/json' };
    return new Response('Not found', { status: 404, headers: cors });
  },
};
