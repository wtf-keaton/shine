import {useState} from 'react'
import './App.css'
import {invoke} from '@shine-ui/api'

function App() {
    const [name, setName] = useState('');
    const [greetMsg, setGreetMsg] = useState("");

    async function handleGreet() {
        const res = await invoke('greet', {name})
        setGreetMsg(res.result);
    }

    return (
        <div className="App">
            <h1>Shine + React = ❤️</h1>
            <div className="card">
                <form
                    className="row"
                    onSubmit={(e) => {
                        e.preventDefault();
                        handleGreet();
                    }}
                >
                    <input
                        id="greet-input"
                        onChange={(e) => setName(e.currentTarget.value)}
                        placeholder="Enter a name..."
                    />
                    <button type="submit">Greet</button>
                </form>

                <p>{greetMsg}</p>
            </div>
        </div>
    )
}

export default App
