import { useState } from 'react'
import { useAlerts } from '../hooks/useSensorData'
import { useTranslation } from 'react-i18next'
import { AlertTriangle, Clock, CheckCircle, AlertCircle } from 'lucide-react'
import { severityColor, timeAgo } from '../lib/utils'

export default function AlertsPage() {
  const { t } = useTranslation('alerts')
  const [filterType, setFilterType] = useState<string>('all')
  const [filterSeverity, setFilterSeverity] = useState<string>('all')
  const acknowledged = filterSeverity === 'pending' ? 0 : filterSeverity === 'resolved' ? 1 : undefined
  const { alerts, loading, acknowledge, refetch } = useAlerts(acknowledged)

  const typeLabels: Record<string, string> = {
    ph_abnormal: t('typePHAbnormal'),
    ec_high: t('typeECHigh'),
    temp_abnormal: t('typeTempAbnormal'),
    do_low: t('typeDOLow'),
    offline: t('typeOffline'),
    water_low: t('typeWaterLow'),
    leak: t('typeLeak'),
  }

  const filteredAlerts = filterType === 'all' ? alerts : alerts.filter((a) => a.type === filterType)

  const pendingCount = alerts.filter((a) => !a.acknowledged).length
  const todayCount = alerts.filter((a) => Date.now() / 1000 - a.created_at < 86400).length
  const weekCount = alerts.filter((a) => Date.now() / 1000 - a.created_at < 604800).length
  const resolvedCount = alerts.filter((a) => a.acknowledged).length

  const statCards = [
    { label: t('pendingAlerts'), value: pendingCount, icon: AlertCircle, color: '#FF9800' },
    { label: t('todayAlerts'), value: todayCount, icon: AlertTriangle, color: '#F44336' },
    { label: t('weekAlerts'), value: weekCount, icon: Clock, color: '#2196F3' },
    { label: t('resolved'), value: resolvedCount, icon: CheckCircle, color: '#4CAF50' },
  ]

  // Pagination
  const [page, setPage] = useState(0)
  const pageSize = 10
  const pageData = filteredAlerts.slice(page * pageSize, (page + 1) * pageSize)
  const totalPages = Math.ceil(filteredAlerts.length / pageSize)

  return (
    <div className="space-y-6">
      <h2 className="text-xl font-bold">{t('title')}</h2>

      {/* Stats Cards */}
      <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-4">
        {statCards.map((card) => (
          <div key={card.label} className="rounded-xl border border-border bg-white p-5 shadow-sm">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-sm text-gray-500">{card.label}</p>
                <p className="mt-1 text-2xl font-bold text-gray-900">{card.value}</p>
              </div>
              <div className="flex h-10 w-10 items-center justify-center rounded-lg" style={{ backgroundColor: `${card.color}15` }}>
                <card.icon className="h-5 w-5" style={{ color: card.color }} />
              </div>
            </div>
          </div>
        ))}
      </div>

      {/* Filters */}
      <div className="flex items-center gap-3">
        <select value={filterType} onChange={(e) => { setFilterType(e.target.value); setPage(0) }} className="rounded-lg border border-border px-3 py-1.5 text-sm">
          <option value="all">{t('allTypes')}</option>
          <option value="ph_abnormal">{t('typePHAbnormal')}</option>
          <option value="ec_high">{t('typeECHigh')}</option>
          <option value="temp_abnormal">{t('typeTempAbnormal')}</option>
          <option value="do_low">{t('typeDOLow')}</option>
          <option value="offline">{t('typeOffline')}</option>
          <option value="water_low">{t('typeWaterLow')}</option>
        </select>
        <select value={filterSeverity} onChange={(e) => { setFilterSeverity(e.target.value); setPage(0) }} className="rounded-lg border border-border px-3 py-1.5 text-sm">
          <option value="all">{t('allStatus')}</option>
          <option value="pending">{t('statusPending')}</option>
          <option value="resolved">{t('statusResolved')}</option>
        </select>
        <button onClick={refetch} className="rounded-lg border border-border px-3 py-1.5 text-sm hover:bg-gray-50">
          {t('common:actions.refresh')}
        </button>
      </div>

      {/* Alert Table */}
      {loading ? (
        <div className="text-sm text-gray-400">{t('common:actions.loading')}</div>
      ) : (
        <div className="rounded-xl border border-border bg-white shadow-sm overflow-hidden">
          <table className="w-full text-sm">
            <thead>
              <tr className="border-b border-border bg-gray-50 text-left text-gray-500">
                <th className="px-5 py-3">{t('severity')}</th>
                <th className="px-5 py-3">{t('type')}</th>
                <th className="px-5 py-3">{t('message')}</th>
                <th className="px-5 py-3">{t('device')}</th>
                <th className="px-5 py-3">{t('time')}</th>
                <th className="px-5 py-3">{t('status')}</th>
                <th className="px-5 py-3">{t('action')}</th>
              </tr>
            </thead>
            <tbody>
              {pageData.map((alert) => (
                <tr key={alert.id} className="border-b border-border/50 hover:bg-gray-50/50">
                  <td className="px-5 py-3">
                    <span className={`rounded px-2 py-0.5 text-xs font-medium ${severityColor(alert.severity)}`}>
                      {alert.severity === 'critical' ? t('sevCritical') : alert.severity === 'warning' ? t('sevWarning') : t('sevInfo')}
                    </span>
                  </td>
                  <td className="px-5 py-3">{typeLabels[alert.type] || alert.type}</td>
                  <td className="px-5 py-3 max-w-xs truncate">{alert.message}</td>
                  <td className="px-5 py-3">{alert.device_name || alert.device_id}</td>
                  <td className="px-5 py-3 text-xs text-gray-400">{timeAgo(alert.created_at)}</td>
                  <td className="px-5 py-3">
                    {alert.acknowledged ? (
                      <span className="text-xs text-green-600">{t('acknowledged')}</span>
                    ) : (
                      <span className="text-xs text-amber-600">{t('statusPending')}</span>
                    )}
                  </td>
                  <td className="px-5 py-3">
                    {!alert.acknowledged && (
                      <button onClick={() => acknowledge(alert.id)} className="rounded bg-[#00a65a] px-2 py-1 text-xs text-white hover:bg-[#00954f]">
                        {t('common:actions.confirm')}
                      </button>
                    )}
                  </td>
                </tr>
              ))}
              {pageData.length === 0 && (
                <tr>
                  <td colSpan={7} className="px-5 py-8 text-center text-gray-400">{t('noAlerts')}</td>
                </tr>
              )}
            </tbody>
          </table>
          {/* Pagination */}
          <div className="flex items-center justify-between px-5 py-3 border-t border-border">
            <span className="text-xs text-gray-400">{t('water:totalRecords', { count: filteredAlerts.length })}，{t('water:page', { current: page + 1, total: totalPages || 1 })}</span>
            <div className="flex gap-1">
              <button onClick={() => setPage(Math.max(0, page - 1))} disabled={page === 0} className="rounded px-2 py-1 text-xs border border-border disabled:opacity-30 hover:bg-gray-50">{t('water:prev')}</button>
              <button onClick={() => setPage(Math.min(totalPages - 1, page + 1))} disabled={page >= totalPages - 1} className="rounded px-2 py-1 text-xs border border-border disabled:opacity-30 hover:bg-gray-50">{t('water:next')}</button>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
