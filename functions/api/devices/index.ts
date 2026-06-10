import type { Env } from '../../_lib/types';

export const onRequest: PagesFunction<Env>[] = [
  async (c) => {
    const url = new URL(c.request.url);
    const db = c.env.DB;
    const officeId = url.searchParams.get('office_id');

    if (c.request.method === 'GET') {
      const now = Math.floor(Date.now() / 1000);
      let query = `SELECT d.*, o.name as office_name,
        (SELECT MAX(t.created_at) FROM telemetry t WHERE t.device_id = d.id) as last_telemetry,
        (SELECT COUNT(*) FROM alerts WHERE device_id = d.id AND acknowledged = 0) as pending_alerts
        FROM devices d LEFT JOIN racks r ON d.id = r.device_id LEFT JOIN offices o ON r.office_id = o.id WHERE 1=1`;
      const params: unknown[] = [];

      if (officeId) {
        query += ` AND (d.id IN (SELECT r.device_id FROM racks r WHERE r.office_id = ? AND r.device_id IS NOT NULL)
                  OR d.id NOT IN (SELECT r2.device_id FROM racks r2 WHERE r2.device_id IS NOT NULL))`;
        params.push(Number(officeId));
      }

      query += ' ORDER BY d.id';

      const result = await db.prepare(query).bind(...params).all<{
        id: string; name: string; status: string; last_seen: number | null;
        last_telemetry: number | null; location: string; floor: number;
        office_name: string | null; pending_alerts: number;
      }>();

      // 用 telemetry 覆蓋 last_seen 和 status
      const enriched = result.results.map(d => ({
        ...d,
        last_seen: d.last_telemetry || d.last_seen,
        status: (d.last_telemetry && d.last_telemetry > now - 300) ? 'online' : 'offline',
      }));

      return new Response(JSON.stringify(enriched), { headers: { 'Content-Type': 'application/json' } });
    }

    return new Response(JSON.stringify({ error: 'Method not allowed' }), { status: 405, headers: { 'Content-Type': 'application/json' } });
  }
];
