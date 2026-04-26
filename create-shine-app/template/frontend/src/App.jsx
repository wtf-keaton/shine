import { useState } from 'react'
import reactLogo from './assets/react.svg'
import viteLogo from './assets/vite.svg'
import heroImg from './assets/hero.png'
import './App.css'
import { invoke } from './shine-api'
function App() {
  const [fileContent, setFileContent] = useState('Нажми кнопку, чтобы прочитать файл через C++')

  const handleReadFile = async () => {
    try {
      const res = await invoke('fs_read_text_file', { path: 'C:/Windows/System32/drivers/etc/hosts' })
      setFileContent(res.content)
    } catch (error) {
      setFileContent('Ошибка: ' + error.message)
    }
  }

  return (
      <div className="App">
        <h1>Shine + React = ❤️</h1>
        <div className="card">
          <button onClick={handleReadFile}>Прочитать файл (Invoke)</button>
        </div>
        <pre style={{ textAlign: 'left', background: '#222', padding: '15px', borderRadius: '8px', overflowX: 'auto' }}>
        {fileContent}
      </pre>
      </div>
  )
}

export default App
