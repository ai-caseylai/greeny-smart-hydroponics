import { NavLink } from 'react-router-dom'
import { useTranslation } from 'react-i18next'
import {
  LayoutDashboard,
  Droplets,
  Cpu,
  Warehouse,
  ChevronDown,
} from 'lucide-react'
import { cn } from '../../lib/utils'
import { useOffice } from '../../context/OfficeContext'
import { useOffices } from '../../hooks/useOffices'
import { AquaGreenLogo } from '../AquaGreenLogo'

export function Sidebar() {
  const { t } = useTranslation()
  const { selectedOfficeId, setSelectedOfficeId, lockedOfficeId, userRole } = useOffice()
  const { offices } = useOffices()

  // Show office selector only for users who aren't locked to an office
  const showOfficeSelector = lockedOfficeId === undefined || lockedOfficeId === null

  const navItems = [
    { to: '/', icon: LayoutDashboard, label: t('nav.dashboard') },
    { to: '/water-quality', icon: Droplets, label: t('nav.waterQuality') },
    { to: '/device-control', icon: Cpu, label: t('nav.deviceControl') },
    { to: '/racks', icon: Warehouse, label: t('nav.racks') },
  ]

  return (
    <aside className="fixed left-0 top-0 z-40 h-screen w-60 bg-[#1B5E20] flex flex-col">
      <div className="flex h-14 items-center px-4 border-b border-white/10">
        <AquaGreenLogo className="h-9 w-auto" white />
      </div>

      {/* Office Selector */}
      {showOfficeSelector && (
        <div className="px-3 pt-3">
          <div className="relative">
            <select
              value={selectedOfficeId ?? ''}
              onChange={(e) => setSelectedOfficeId(e.target.value ? Number(e.target.value) : null)}
              className="w-full appearance-none rounded-lg bg-[#2E7D32] px-3 py-2 pr-8 text-sm text-white outline-none hover:bg-[#388E3C] cursor-pointer"
            >
              <option value="">{t('racks:allOffices')}</option>
              {offices.map((o) => (
                <option key={o.id} value={o.id}>{o.name}</option>
              ))}
            </select>
            <ChevronDown className="pointer-events-none absolute right-2 top-2.5 h-4 w-4 text-green-300" />
          </div>
        </div>
      )}

      <nav className="flex-1 p-3 space-y-1">
        {navItems.map((item) => (
          <NavLink
            key={item.to}
            to={item.to}
            end={item.to === '/'}
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
      <div className="p-3 border-t border-white/10 text-xs text-green-300/70">
        Greeny Smart Hydroponics
      </div>
    </aside>
  )
}
