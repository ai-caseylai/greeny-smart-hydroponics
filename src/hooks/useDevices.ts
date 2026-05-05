import { useState, useEffect, useCallback } from 'react'
import { apiFetch } from '../lib/api'
import type { Device } from '../types'

export function useDevices(officeId?: number | null) {
  const [devices, setDevices] = useState<Device[]>([])
  const [loading, setLoading] = useState(true)

  const fetchDevices = useCallback(async () => {
    try {
      const params = officeId ? `?office_id=${officeId}` : ''
      const data = await apiFetch<Device[]>(`/devices${params}`)
      setDevices(data)
    } catch {
      // handled by apiFetch
    } finally {
      setLoading(false)
    }
  }, [officeId])

  useEffect(() => { fetchDevices() }, [fetchDevices])

  return { devices, loading, refetch: fetchDevices }
}
