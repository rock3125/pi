import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import '@fontsource-variable/jetbrains-mono'
import '@fontsource-variable/archivo'
import './styles.css'
import { App } from './App'

const root = document.getElementById('root')
if (!root) throw new Error('piview: no #root to mount into')

createRoot(root).render(
  <StrictMode>
    <App />
  </StrictMode>,
)
