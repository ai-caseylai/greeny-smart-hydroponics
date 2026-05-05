import { useState, useEffect, useCallback } from 'react'
import { apiFetch } from '../lib/api'
import type { DashboardStats, Telemetry, Alert } from '../types'

export function useDashboardStats(officeId?: number | null) {
  const [stats, setStats] = useState<DashboardStats | null>(null)
  const [loading, setLoading] = useState(true)

  const fetchStats = useCallback(async () => {
    try {
      const params = officeId ? `?office_id=${officeId}` : ''
      const data = await apiFetch<DashboardStats>(`/dashboard/stats${params}`)
      setStats(data)
    } catch {
      // handled by apiFetch
    } finally {
      setLoading(false)
    }
  }, [officeId])

  useEffect(() => { fetchStats() }, [fetchStats])

  return { stats, loading, refetch: fetchStats }
}

export function useTelemetry(deviceId?: string, limit = 100, officeId?: number | null) {
  const [data, setData] = useState<Telemetry[]>([])
  const [loading, setLoading] = useState(true)

  const fetchData = useCallback(async () => {
    try {
      const params = new URLSearchParams()
      if (deviceId) params.set('device_id', deviceId)
      if (officeId) params.set('office_id', String(officeId))
      params.set('limit', String(limit))
      const result = await apiFetch<Telemetry[]>(`/telemetry?${params}`)
      setData(result)
    } catch {
      // handled by apiFetch
    } finally {
      setLoading(false)
    }
  }, [deviceId, limit, officeId])

  useEffect(() => { fetchData() }, [fetchData])

  return { data, loading, refetch: fetchData }
}

export function useAlerts(acknowledged?: number) {
  const [alerts, setAlerts] = useState<Alert[]>([])
  const [loading, setLoading] = useState(true)

  const fetchAlerts = useCallback(async () => {
    try {
      const params = new URLSearchParams()
      if (acknowledged !== undefined) params.set('acknowledged', String(acknowledged))
      const data = await apiFetch<Alert[]>(`/alerts?${params}`)
      setAlerts(data)
    } catch {
      // handled by apiFetch
    } finally {
      setLoading(false)
    }
  }, [acknowledged])

  useEffect(() => { fetchAlerts() }, [fetchAlerts])

  const acknowledge = useCallback(async (id: number) => {
    await apiFetch(`/alerts/${id}`, {
      method: 'PATCH',
      body: JSON.stringify({ acknowledged: 1 }),
    })
    fetchAlerts()
  }, [fetchAlerts])

  return { alerts, loading, refetch: fetchAlerts, acknowledge }
}
