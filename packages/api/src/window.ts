import { invoke } from './core.js';

export const appWindow = {
    startDrag: async (): Promise<void> => {
        await invoke<void>('window_drag');
    }
};