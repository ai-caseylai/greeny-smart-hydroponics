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
      const id = env.DEVICE_HUB.idFromName(`office-${officeId}`);
      const stub = env.DEVICE_HUB.get(id);
      return stub.fetch(request);
    }

    return new Response('Not found', { status: 404 });
  },
};
