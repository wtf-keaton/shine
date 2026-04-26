import { invoke } from './core.js';

export async function readTextFile(path: string): Promise<string> {
    const response = await invoke<{content: string}>('fs_read_text_file', { path });
    return response.content;
}