import { invoke } from './core.js';

export async function readFile(path: string): Promise<string> {
    const response = await invoke<{content: string}>('fs_read_file', { path });
    return response.content;
}