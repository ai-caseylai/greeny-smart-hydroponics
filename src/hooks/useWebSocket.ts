import { useEffect, useRef, useCallback, useState } from 'react'
import { getToken } from '../lib/api'

interface TelemetryUpdate {
  device_id: string
  data: {
    ph: number; ec: number; water_temp: number; water_level: number
    ndvi: number; spectral_red: number; spectral_green: number
    spectral_blue: number; spectral_nir: number
    relay1: number; relay2: number; relay3: number; relay4: number
  }
  ts_ms: number
}

interface DeviceStatus {
  device_id: string
  status: 'online' | 'offline'
}

type MessageHandler = (data: any) => void

export function useWebSocket(officeId: number | null) {
  const wsRef = useRef<WebSocket | null>(null)
  const handlersRef = useRef<Map<string, Set<MessageHandler>>>(new Map())
  const [connected, setConnected] = useState(false)
  const reconnectTimerRef = useRef<number>(0)

  const connect = useCallback(() => {
    if (!officeId) return

    const token = getToken()
    if (!token) return

    const wsBase = import.meta.env.VITE_WS_URL || 'wss://greeny-ws.techforliving.net'
    const wsUrl = `${wsBase}/ws?dashboard=1&office_id=${officeId}&token=${token}`

    const ws = new WebSocket(wsUrl)
    wsRef.current = ws

    ws.onopen = () => {
      setConnected(true)
      const pingInterval = setInterval(() => {
        if (ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({ type: 'ping' }))
        }
      }, 30000)
      ws.addEventListener('close', () => clearInterval(pingInterval), { once: true })
    }

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data)
        const handlers = handlersRef.current.get(msg.type)
        if (handlers) {
          handlers.forEach(handler => handler(msg))
        }
      } catch { /* ignore */ }
    }

    ws.onclose = () => {
      setConnected(false)
      wsRef.current = null
      clearTimeout(reconnectTimerRef.current)
      reconnectTimerRef.current = window.setTimeout(connect, 3000)
    }

    ws.onerror = () => {
      ws.close()
    }
  }, [officeId])

  useEffect(() => {
    connect()
    return () => {
      clearTimeout(reconnectTimerRef.current)
      if (wsRef.current) {
        wsRef.current.close()
        wsRef.current = null
      }
    }
  }, [connect])

  const on = useCallback((type: string, handler: MessageHandler) => {
    if (!handlersRef.current.has(type)) {
      handlersRef.current.set(type, new Set())
    }
    handlersRef.current.get(type)!.add(handler)
    return () => {
      handlersRef.current.get(type)?.delete(handler)
    }
  }, [])

  const sendRelay = useCallback((deviceId: string, relay1: number, relay2: number, relay3?: number, relay4?: number) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      const msg: any = { type: 'relay', device_id: deviceId, relay1, relay2 }
      if (relay3 !== undefined) msg.relay3 = relay3
      if (relay4 !== undefined) msg.relay4 = relay4
      wsRef.current.send(JSON.stringify(msg))
    }
  }, [])

  const sendPhCal = useCallback((deviceId: string, phCal: number) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'relay',
        device_id: deviceId,
        ph_cal: phCal,
      }))
    }
  }, [])

  return { connected, on, sendRelay, sendPhCal }
}
