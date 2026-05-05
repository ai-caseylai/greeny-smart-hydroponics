import type { Env } from '../../_lib/types';

export const onRequest: PagesFunction<Env>[] = [
  async (c) => {
    const url = new URL(c.request.url);
    const db = c.env.DB;
    const now = Math.floor(Date.now() / 1000);
    const todayStart = now - (now % 86400);
    const officeId = url.searchParams.get('office_id');

    // Build office filter: JOIN racks to link devices to offices
    const officeJoin = officeId
      ? ' AND d.id IN (SELECT r.device_id FROM racks r WHERE r.office_id = ? AND r.device_id IS NOT NULL)'
      : '';

    const bindOffice = (params: unknown[]) => officeId ? [...params, Number(officeId)] : params;

    const [onlineDevices, totalDevices, todayAlerts, avgPh, avgTemp, avgNdvi, statusDist, recentAlerts] = await Promise.all([
      db.prepare(`SELECT COUNT(*) as count FROM devices d WHERE d.status = 'online' AND d.last_seen > ?${officeJoin}`)
        .bind(...bindOffice([now - 300])).first<{ count: number }>(),
      db.prepare(`SELECT COUNT(*) as count FROM devices d WHERE 1=1${officeJoin}`)
        .bind(...bindOffice([])).first<{ count: number }>(),
      db.prepare(`SELECT COUNT(*) as count FROM alerts WHERE created_at >= ?${officeId ? ' AND device_id IN (SELECT r.device_id FROM racks r WHERE r.office_id = ? AND r.device_id IS NOT NULL)' : ''}`)
        .bind(...bindOffice([todayStart])).first<{ count: number }>(),
      db.prepare(`SELECT AVG(t.ph) as avg FROM telemetry t JOIN devices d ON t.device_id = d.id WHERE t.created_at >= ?${officeJoin}`)
        .bind(...bindOffice([todayStart])).first<{ avg: number }>(),
      db.prepare(`SELECT AVG(t.water_temp) as avg FROM telemetry t JOIN devices d ON t.device_id = d.id WHERE t.created_at >= ?${officeJoin}`)
        .bind(...bindOffice([todayStart])).first<{ avg: number }>(),
      db.prepare(`SELECT AVG(t.ndvi) as avg FROM telemetry t JOIN devices d ON t.device_id = d.id WHERE t.created_at >= ? AND t.ndvi > 0${officeJoin}`)
        .bind(...bindOffice([todayStart])).first<{ avg: number }>(),
      db.prepare(`SELECT d.status, COUNT(*) as count FROM devices d WHERE 1=1${officeJoin} GROUP BY d.status`)
        .bind(...bindOffice([])).all(),
      db.prepare(`SELECT a.*, d.name as device_name FROM alerts a LEFT JOIN devices d ON a.device_id = d.id
                  WHERE a.acknowledged = 0${officeId ? ' AND a.device_id IN (SELECT r.device_id FROM racks r WHERE r.office_id = ? AND r.device_id IS NOT NULL)' : ''} ORDER BY a.created_at DESC LIMIT 10`)
        .bind(...bindOffice([])).all(),
    ]);

    return new Response(JSON.stringify({
      online_devices: onlineDevices?.count || 0,
      total_devices: totalDevices?.count || 0,
      today_alerts: todayAlerts?.count || 0,
      avg_ph: Math.round((avgPh?.avg || 0) * 10) / 10,
      avg_temp: Math.round((avgTemp?.avg || 0) * 10) / 10,
      avg_ndvi: Math.round((avgNdvi?.avg || 0) * 100) / 100,
      device_distribution: statusDist.results,
      recent_alerts: recentAlerts.results,
    }), { headers: { 'Content-Type': 'application/json' } });
  }
];
