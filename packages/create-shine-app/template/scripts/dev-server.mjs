import fs from 'node:fs';
import http from 'node:http';
import net from 'node:net';
import { spawn } from 'node:child_process';
import process from 'node:process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const projectRoot = path.resolve(__dirname, '..');
const frontendDir = path.join(projectRoot, 'frontend');
const stateDir = path.resolve(process.argv[3] || path.join(projectRoot, '.shine'));
const stateFile = path.join(stateDir, 'vite-dev-server.json');

const HOST = '127.0.0.1';
const PORT = 1745;
const SERVER_URL = `http://${HOST}:${PORT}/`;
const START_TIMEOUT_MS = 15000;
const CHECK_INTERVAL_MS = 250;

const npmExe = path.normalize(process.argv[2] || 'npm');

function delay(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

function readState() {
    try {
        return JSON.parse(fs.readFileSync(stateFile, 'utf8'));
    } catch {
        return null;
    }
}

function writeState(state) {
    fs.mkdirSync(stateDir, { recursive: true });
    fs.writeFileSync(stateFile, JSON.stringify(state, null, 2), 'utf8');
}

function removeState() {
    try {
        fs.rmSync(stateFile, { force: true });
    } catch {
        // Best effort cleanup only.
    }
}

function isProcessAlive(pid) {
    if (!Number.isInteger(pid) || pid <= 0) return false;

    try {
        process.kill(pid, 0);
        return true;
    } catch {
        return false;
    }
}

function requestServer() {
    return new Promise((resolve) => {
        const req = http.get(SERVER_URL, (res) => {
            res.resume();
            resolve({
                ok: true,
                vite: typeof res.headers['x-vite-error'] !== 'undefined'
                    || typeof res.headers['x-powered-by'] !== 'undefined'
                    || typeof res.headers['cache-control'] !== 'undefined'
            });
        });

        req.setTimeout(1000, () => {
            req.destroy();
            resolve({ ok: false, vite: false });
        });

        req.on('error', () => resolve({ ok: false, vite: false }));
    });
}

function isPortInUse(port) {
    return new Promise((resolve) => {
        const server = net.createServer();
        server.once('error', (err) => {
            resolve(err.code === 'EADDRINUSE');
        });
        server.once('listening', () => {
            server.close(() => resolve(false));
        });
        server.listen(port, HOST);
    });
}

async function waitForServer(timeoutMs) {
    const deadline = Date.now() + timeoutMs;

    while (Date.now() < deadline) {
        const status = await requestServer();
        if (status.ok) return true;
        await delay(CHECK_INTERVAL_MS);
    }

    return false;
}

async function ensureServerNotAlreadyRunning() {
    const status = await requestServer();
    if (status.ok) {
        console.log(`[Shine] Vite dev server is already responding at ${SERVER_URL}.`);
        return true;
    }

    const state = readState();
    if (state?.pid && isProcessAlive(state.pid)) {
        console.log(`[Shine] Vite dev server process ${state.pid} is still starting. Waiting...`);
        if (await waitForServer(START_TIMEOUT_MS)) {
            console.log(`[Shine] Vite dev server is ready at ${SERVER_URL}.`);
            return true;
        }

        console.warn(`[Shine] Stored Vite process ${state.pid} did not become ready. Clearing stale state.`);
        removeState();
    }

    return false;
}

async function main() {
    if (await ensureServerNotAlreadyRunning()) {
        return;
    }

    if (await isPortInUse(PORT)) {
        throw new Error(`[Shine] Port ${PORT} is already in use, but ${SERVER_URL} is not responding.`);
    }

    console.log(`[Shine] Starting Vite dev server at ${SERVER_URL}...`);

    fs.mkdirSync(stateDir, { recursive: true });

    const outLog = fs.openSync(path.join(stateDir, 'vite-out.log'), 'a');
    const errLog = fs.openSync(path.join(stateDir, 'vite-err.log'), 'a');
    console.log(`[Shine] Detailed logs are being written to: ${path.join(stateDir, 'vite-err.log')}`);

    const isWin = process.platform === 'win32';
    const executable = isWin ? (process.env.comspec || 'cmd.exe') : npmExe;

    const args = isWin
        ? ['/c', `"${npmExe}" run dev -- --host ${HOST} --port ${PORT}`]
        : ['run', 'dev', '--', '--host', HOST, '--port', PORT.toString()];
    const child = spawn(executable, args, {
        cwd: frontendDir,
        detached: true,
        stdio: ['ignore', outLog, errLog],
        windowsHide: true,
        windowsVerbatimArguments: isWin,
        shell: false
    });

    child.unref();
    writeState({
        pid: child.pid,
        port: PORT,
        url: SERVER_URL,
        startedAt: new Date().toISOString()
    });

    if (!(await waitForServer(START_TIMEOUT_MS))) {
        throw new Error(`[Shine] Timed out waiting for Vite dev server at ${SERVER_URL}. Check vite-err.log for details!`);
    }

    console.log(`[Shine] Vite dev server is ready at ${SERVER_URL}.`);
}

main().catch((error) => {
    console.error(error.message || error);
    process.exit(1);
});
