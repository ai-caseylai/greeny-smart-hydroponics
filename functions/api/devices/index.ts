import type { Env } from '../../_lib/types';

export const onRequest: PagesFunction<Env>[] = [
  async (c) => {
    const url = new URL(c.request.url);
    const db = c.env.DB;
    const officeId = url.searchParams.get('office_id');

    if (c.request.method === 'GET') {
      let query = `SELECT d.*, o.name as office_name, (SELECT COUNT(*) FROM alerts WHERE device_id = d.id AND acknowledged = 0) as pending_alerts
        FROM devices d LEFT JOIN racks r ON d.id = r.device_id LEFT JOIN offices o ON r.office_id = o.id WHERE 1=1`;
      const params: unknown[] = [];

      if (officeId) {
        query += ' AND d.id IN (SELECT r.device_id FROM racks r WHERE r.office_id = ? AND r.device_id IS NOT NULL)';
        params.push(Number(officeId));
      }

      query += ' ORDER BY d.id';

      const result = await db.prepare(query).bind(...params).all();
      return new Response(JSON.stringify(result.results), { headers: { 'Content-Type': 'application/json' } });
    }

    return new Response(JSON.stringify({ error: 'Method not allowed' }), { status: 405, headers: { 'Content-Type': 'application/json' } });
  }
];
