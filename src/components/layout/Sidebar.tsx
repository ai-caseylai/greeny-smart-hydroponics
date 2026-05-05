import { NavLink } from 'react-router-dom'
import { useTranslation } from 'react-i18next'
import {
  LayoutDashboard,
  Droplets,
  Cpu,
  Warehouse,
  Users,
  Sprout,
  FileText,
  Package,
  X,
} from 'lucide-react'
import { cn } from '../../lib/utils'
import { useOffice } from '../../context/OfficeContext'
import { useOffices } from '../../hooks/useOffices'
import { AquaGreenLogo } from '../AquaGreenLogo'

export function Sidebar({ onNavClick }: { onNavClick?: () => void }) {
  const { t } = useTranslation()
  const { selectedOfficeId, setSelectedOfficeId, lockedOfficeId, userRole } = useOffice()
  const { offices } = useOffices()

  const navItems = [
    { to: '/', icon: LayoutDashboard, label: t('nav.dashboard') },
    { to: '/water-quality', icon: Droplets, label: t('nav.waterQuality') },
    { to: '/device-control', icon: Cpu, label: t('nav.deviceControl') },
    { to: '/racks', icon: Warehouse, label: t('nav.racks') },
    { to: '/crops', icon: Sprout, label: t('nav.crops', { defaultValue: '農作物管理' }) },
    ...(userRole !== 'staff' ? [{ to: '/users', icon: Users, label: t('nav.users', { defaultValue: '人員管理' }) }] : []),
  ]

  return (
    <aside className="h-full w-60 bg-[#1B5E20] flex flex-col">
      <div className="flex h-14 items-center justify-between px-4 border-b border-white/10">
        <div className="flex items-center">
          <AquaGreenLogo className="h-9 w-auto" white />
          <span className="ml-2 text-xs text-green-300 font-medium leading-tight hidden sm:inline">水耕架管理系統</span>
        </div>
        <button onClick={onNavClick} className="lg:hidden text-green-300 hover:text-white p-1">
          <X className="h-5 w-5" />
        </button>
      </div>

      <nav className="flex-1 p-3 space-y-1">
        {navItems.map((item) => (
          <NavLink
            key={item.to}
            to={item.to}
            end={item.to === '/'}
            onClick={onNavClick}
            className={({ isActive }) =>
              cn(
                'flex items-center gap-3 rounded-lg px-3 py-2.5 text-sm transition-colors',
                isActive
                  ? 'bg-[#2E7D32] text-white font-medium'
                  : 'text-green-200 hover:bg-white/10 hover:text-white'
              )
            }
          >
            <item.icon className="h-4 w-4" />
            {item.label}
          </NavLink>
        ))}
      </nav>
      <div className="p-3 border-t border-white/10 space-y-1">
        <a
          href="/Greeny_使用手冊.md"
          target="_blank"
          onClick={onNavClick}
          className="flex items-center gap-3 rounded-lg px-3 py-2 text-xs text-green-300 hover:bg-white/10 hover:text-white transition-colors"
        >
          <FileText className="h-4 w-4" />
          使用手冊 / Manual
        </a>
        <a
          href="/greeny-skill.zip"
          download
          onClick={onNavClick}
          className="flex items-center gap-3 rounded-lg px-3 py-2 text-xs text-green-300 hover:bg-white/10 hover:text-white transition-colors"
        >
          <Package className="h-4 w-4" />
          Skill 文件
        </a>
      </div>
      <div className="px-4 pb-3 text-[10px] text-green-400/50">
        Greenie Smart Hydroponics
      </div>
    </aside>
  )
}
