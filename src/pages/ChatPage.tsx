import { useState, useRef, useEffect, type FormEvent } from 'react'
import { useTranslation } from 'react-i18next'
import { Send, Bot, User, Droplets, Leaf, Heart, Bug, Scissors, FlaskConical } from 'lucide-react'

interface Message {
  role: 'user' | 'assistant'
  content: string
}

export default function ChatPage() {
  const { t } = useTranslation('chat')
  const [messages, setMessages] = useState<Message[]>([
    { role: 'assistant', content: t('welcome') },
  ])
  const [input, setInput] = useState('')
  const [sending, setSending] = useState(false)
  const bottomRef = useRef<HTMLDivElement>(null)

  const quickActions = [
    { label: t('quickActions.watering'), icon: Droplets },
    { label: t('quickActions.repot'), icon: Leaf },
    { label: t('quickActions.health'), icon: Heart },
    { label: t('quickActions.disease'), icon: Bug },
    { label: t('quickActions.pruning'), icon: Scissors },
    { label: t('quickActions.nutrition'), icon: FlaskConical },
  ]

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [messages])

  const sendMessage = async (text: string) => {
    if (!text.trim() || sending) return

    const userMsg: Message = { role: 'user', content: text.trim() }
    setMessages((prev) => [...prev, userMsg])
    setInput('')
    setSending(true)

    // Simulated AI response
    setTimeout(() => {
      const replies: Record<string, string> = {
        [t('quickActions.watering')]: '目前已啟用自動澆水模式。所有感測器水位正常，A區水位 85%、B區水位 78%。下次排程澆水時間：今日 18:00。',
        [t('quickActions.repot')]: '目前有 3 株植物建議換盆：\n• A區-03 羅勒（根系已滿）\n• C區-07 薄荷（介質老化）\n• D區-02 番茄（根系過密）\n是否需要建立換盆任務？',
        [t('quickActions.health')]: '目前監測的 45 株植物健康狀態：\n• 健康：38 株 (84%)\n• 需關注：5 株 (11%)\n• 異常：2 株 (5%)\n異常植株位於 D區，建議檢查營養液 EC 值。',
        [t('quickActions.disease')]: '目前偵測到 1 例可能的葉斑病，位於 E區-05 生菜。建議處理方式：\n1. 隔離受感染植株\n2. 調整環境濕度至 60% 以下\n3. 使用稀釋的 neem oil 噴灑',
        [t('quickActions.pruning')]: '本週建議剪枝的植物：\n• B區-02 九層塔（頂芽過長）\n• C區-04 迷迭香（枝條過密）\n• E區-01 薄荷（控制擴張）\n剪枝有助於促進分枝生長。',
        [t('quickActions.nutrition')]: '目前營養液狀態：\n• A區營養液：EC 1200 μS/cm，pH 6.2（正常）\n• B區營養液：EC 2100 μS/cm，pH 7.8（需調整！）\n• C區營養液：EC 1250 μS/cm，pH 6.1（正常）\nB區需要更換營養液。',
      }

      const reply: Message = {
        role: 'assistant',
        content: replies[text.trim()] || `[模擬回覆] 收到你的問題：「${text.trim()}」\n\n目前 AI 功能尚未連接後端 LLM，完成整合後將可查詢設備狀態並提供專業建議。`,
      }
      setMessages((prev) => [...prev, reply])
      setSending(false)
    }, 800)
  }

  const handleSubmit = (e: FormEvent) => {
    e.preventDefault()
    sendMessage(input)
  }

  return (
    <div className="space-y-4">
      <h2 className="text-xl font-bold">{t('title')}</h2>

      <div className="rounded-xl border border-border bg-white shadow-sm flex flex-col" style={{ height: 'calc(100vh - 200px)' }}>
        {/* Messages */}
        <div className="flex-1 overflow-y-auto p-4 space-y-4">
          {messages.map((msg, i) => (
            <div key={i} className={`flex gap-3 ${msg.role === 'user' ? 'justify-end' : ''}`}>
              {msg.role === 'assistant' && (
                <div className="flex h-8 w-8 shrink-0 items-center justify-center rounded-full bg-[#e8f5e9]">
                  <Bot className="h-4 w-4 text-[#2E7D32]" />
                </div>
              )}
              <div
                className={`max-w-[70%] rounded-xl px-4 py-2.5 text-sm whitespace-pre-wrap ${
                  msg.role === 'user'
                    ? 'bg-[#00a65a] text-white'
                    : 'bg-gray-100 text-gray-800'
                }`}
              >
                {msg.content}
              </div>
              {msg.role === 'user' && (
                <div className="flex h-8 w-8 shrink-0 items-center justify-center rounded-full bg-[#00a65a]">
                  <User className="h-4 w-4 text-white" />
                </div>
              )}
            </div>
          ))}
          {sending && (
            <div className="flex gap-3">
              <div className="flex h-8 w-8 shrink-0 items-center justify-center rounded-full bg-[#e8f5e9]">
                <Bot className="h-4 w-4 text-[#2E7D32]" />
              </div>
              <div className="rounded-xl bg-gray-100 px-4 py-2.5 text-sm text-gray-400">
                {t('thinking')}
              </div>
            </div>
          )}
          <div ref={bottomRef} />
        </div>

        {/* Quick Actions */}
        <div className="border-t border-border px-4 py-3">
          <div className="flex gap-2 overflow-x-auto pb-1">
            {quickActions.map((action) => (
              <button
                key={action.label}
                onClick={() => sendMessage(action.label)}
                disabled={sending}
                className="flex shrink-0 items-center gap-1.5 rounded-full border border-[#00a65a]/30 bg-[#e8f5e9] px-3 py-1.5 text-xs text-[#2E7D32] hover:bg-[#c8e6c9] disabled:opacity-50 transition-colors"
              >
                <action.icon className="h-3 w-3" />
                {action.label}
              </button>
            ))}
          </div>
        </div>

        {/* Input */}
        <form onSubmit={handleSubmit} className="border-t border-border p-4 flex gap-3">
          <input
            value={input}
            onChange={(e) => setInput(e.target.value)}
            placeholder={t('placeholder')}
            className="flex-1 rounded-lg border border-border px-3 py-2 text-sm outline-none focus:border-[#00a65a] focus:ring-2 focus:ring-[#00a65a]/20"
          />
          <button
            type="submit"
            disabled={sending || !input.trim()}
            className="rounded-lg bg-[#00a65a] px-4 py-2 text-sm text-white hover:bg-[#00954f] disabled:opacity-50"
          >
            <Send className="h-4 w-4" />
          </button>
        </form>
      </div>
    </div>
  )
}
