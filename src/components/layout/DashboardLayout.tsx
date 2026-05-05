import { Outlet } from 'react-router-dom'
import { Sidebar } from './Sidebar'
import { Header } from './Header'
import { OfficeProvider } from '../../context/OfficeContext'

export function DashboardLayout() {
  return (
    <OfficeProvider>
      <div className="min-h-screen bg-background">
        <Sidebar />
        <div className="pl-60">
          <Header />
          <main className="p-6">
            <Outlet />
          </main>
        </div>
      </div>
    </OfficeProvider>
  )
}
