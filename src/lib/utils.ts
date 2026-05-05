import { clsx, type ClassValue } from 'clsx'
import { twMerge } from 'tailwind-merge'

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}

export function formatTime(unixSeconds: number): string {
  return new Date(unixSeconds * 1000).toLocaleString('zh-TW')
}

export function timeAgo(unixSeconds: number): string {
  const diff = Math.floor(Date.now() / 1000) - unixSeconds
  if (diff < 60) return `${diff} 秒前`
  if (diff < 3600) return `${Math.floor(diff / 60)} 分鐘前`
  if (diff < 86400) return `${Math.floor(diff / 3600)} 小時前`
  return `${Math.floor(diff / 86400)} 天前`
}

export function severityColor(severity: string): string {
  switch (severity) {
    case 'critical': return 'text-red-700 bg-red-100'
    case 'warning': return 'text-amber-700 bg-amber-100'
    case 'info': return 'text-blue-700 bg-blue-100'
    default: return 'text-gray-700 bg-gray-100'
  }
}

export function statusColor(status: string): string {
  switch (status) {
    case 'online': return 'bg-green-500'
    case 'warning': return 'bg-amber-500'
    case 'alarm': return 'bg-red-500'
    case 'maintenance': return 'bg-blue-500'
    case 'offline': return 'bg-gray-400'
    default: return 'bg-gray-400'
  }
}
