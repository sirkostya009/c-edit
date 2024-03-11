// const toggleTerminalBtn = document.getElementById('toggleTerminalBtn');
// const terminal = document.getElementById('terminal');

// toggleTerminalBtn.addEventListener('click', () => {
//     console.log('clicked');
//     terminal.classList.toggle('hidden');
//     dispatchEvent(new KeyboardEvent('resize'));
// });


// Toggle dropdown visibility on button click
document.getElementById('menu-button').addEventListener('click', function () {
    var dropdownMenu = document.getElementById('dropdown-menu');
    dropdownMenu.classList.toggle('hidden');
});

// Close the dropdown when clicking outside of it
window.addEventListener('click', function (event) {
    var menuButton = document.getElementById('menu-button');
    var dropdownMenu = document.getElementById('dropdown-menu');

    if (!menuButton.contains(event.target) && !dropdownMenu.contains(event.target)) {
        dropdownMenu.classList.add('hidden');
    }
});

