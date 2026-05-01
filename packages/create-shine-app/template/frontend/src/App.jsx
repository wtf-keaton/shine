import { useState } from 'react';
import { invoke } from '@shine-ui/api';
import './App.css';

function App() {
    const [name, setName] = useState('');
    const [greetMsg, setGreetMsg] = useState('');
    const [isLoading, setIsLoading] = useState(false);

    async function handleGreet() {
        if (!name.trim()) return;

        setIsLoading(true);
        try {
            const res = await invoke('greet', { name });
            setGreetMsg(res.result || res);
        } catch (error) {
            setGreetMsg("Error: Could not connect to Shine Core");
        } finally {
            setIsLoading(false);
        }
    }

    return (
        <main className="container">
            <div className="hero">
                <h1 className="title">Welcome to Shine</h1>
                <p className="subtitle">High-Performance C++ & React Framework</p>
            </div>

            <div className="card">
                <form
                    className="greet-form"
                    onSubmit={(e) => {
                        e.preventDefault();
                        handleGreet();
                    }}
                >
                    <input
                        id="greet-input"
                        value={name}
                        onChange={(e) => setName(e.currentTarget.value)}
                        placeholder="Enter your name..."
                        autoComplete="off"
                    />
                    <button type="submit" disabled={isLoading || !name.trim()}>
                        {isLoading ? 'Sending...' : 'Greet'}
                    </button>
                </form>

                <div className={`message-box ${greetMsg ? 'visible' : ''}`}>
                    <p>{greetMsg}</p>
                </div>
            </div>

            <footer className="footer">
                Edit <code>src/App.jsx</code> to test HMR
            </footer>
        </main>
    );
}

export default App;