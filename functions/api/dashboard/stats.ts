import type { Env } from '../../_lib/types';

export const onRequest: PagesFunction<Env>[] = [
  async (c) => {
    const url = new URL(c.request.url);
    const db = c.env.DB;
    const now = Math.floor(Date.now() / 1000);
    const todayStart = now - (now % 86400);
    const officeId = url.searchParams.get('office_id');

    const officeJoin = officeId
      ? ` AND (d.id IN (SELECT r.device_id FROM racks r WHERE r.office_id = ? AND r.device_id IS NOT NULL) OR d.id NOT IN (SELECT r2.device_id FROM racks r2 WHERE r2.device_id IS NOT NULL))`
      : '';

    const bindOffice = (params: unknown[]) => officeId ? [...params, Number(officeId)] : params;

    // "線上" = 最近 5 分鐘有 telemetry 或有 WSS 連線
    const onlineQuery = officeId
      ? `SELECT COUNT(DISTINCT d.id) as count FROM devices d
         WHERE (d.last_seen > ? OR d.id IN (SELECT DISTINCT t.device_id FROM telemetry t WHERE t.created_at > ?))
         AND (d.id IN (SELECT r.device_id FROM racks r WHERE r.office_id = ? AND r.device_id IS NOT NULL)
              OR d.id NOT IN (SELECT r2.device_id FROM racks r2 WHERE r2.device_id IS NOT NULL))`
      : `SELECT COUNT(DISTINCT d.id) as count FROM devices d
         WHERE d.last_seen > ? OR d.id IN (SELECT DISTINCT t.device_id FROM telemetry t WHERE t.created_at > ?)`;

    const [onlineDevices, totalDevices, todayAlerts, avgPh, avgTemp, avgEc, statusDist, recentAlerts] = await Promise.all([
      officeId
        ? db.prepare(onlineQuery).bind(now - 300, now - 300, Number(officeId)).first<{ count: number }>()
        : db.prepare(onlineQuery).bind(now - 300, now - 300).first<{ count: number }>(),
      db.prepare(`SELECT COUNT(*) as count FROM devices d WHERE 1=1${officeJoin}`)
        .bind(...bindOffice([])).first<{ count: number }>(),
      db.prepare(`SELECT COUNT(*) as count FROM alerts WHERE created_at >= ?`)
        .bind(todayStart).first<{ count: number }>(),
      db.prepare(`SELECT AVG(t.ph) as avg FROM telemetry t WHERE t.created_at >= ?`)
        .bind(todayStart).first<{ avg: number }>(),
      db.prepare(`SELECT AVG(t.water_temp) as avg FROM telemetry t WHERE t.created_at >= ?`)
        .bind(todayStart).first<{ avg: number }>(),
      db.prepare(`SELECT AVG(t.ec) as avg FROM telemetry t WHERE t.created_at >= ?`)
        .bind(todayStart).first<{ avg: number }>(),
      db.prepare(`SELECT d.status, COUNT(*) as count FROM devices d WHERE 1=1${officeJoin} GROUP BY d.status`)
        .bind(...bindOffice([])).all(),
      db.prepare(`SELECT a.*, d.name as device_name FROM alerts a LEFT JOIN devices d ON a.device_id = d.id
                  WHERE a.acknowledged = 0 ORDER BY a.created_at DESC LIMIT 10`).all(),
    ]);

    return new Response(JSON.stringify({
      online_devices: onlineDevices?.count || 0,
      total_devices: totalDevices?.count || 0,
      today_alerts: todayAlerts?.count || 0,
      avg_ph: Math.round((avgPh?.avg || 0) * 10) / 10,
      avg_temp: Math.round((avgTemp?.avg || 0) * 10) / 10,
      avg_ec: Math.round(avgEc?.avg || 0),
      device_distribution: statusDist.results,
      recent_alerts: recentAlerts.results,
    }), { headers: { 'Content-Type': 'application/json' } });
  }
];
