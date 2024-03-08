const toggleTerminalBtn = document.getElementById('toggleTerminalBtn');
const terminal = document.getElementById('terminal');

toggleTerminalBtn.addEventListener('click', () => {
    console.log('clicked');
    terminal.classList.toggle('hidden');
    dispatchEvent(new KeyboardEvent('resize'));
});
