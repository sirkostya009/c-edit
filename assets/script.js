'use strict';

const terminalSection = document.getElementById('terminal-section');
const terminal = document.getElementById('terminal');
const codearea = document.getElementById('codearea');
const tabs = document.getElementById('tabs');
const status = document.getElementById('status');

function keyBindsListener(e) {
    if (e.ctrlKey && e.key === 's' && codearea.hasAttribute('readonly')) {
        if (activeTab && !activeTab.getAttribute('data-file')) {
            const filename = SaveFileDialog(codearea.value);
            if (filename) {
                activeTab.getAttribute('data-file', filename);
                const groups = /.*([\/\\](.*))$/.exec(filename);
                activeTab.children[1].innerText = groups[2];
            }
            activeTab.setAttribute('data-file', filename);
        } else if (activeTab) {
            const filename = activeTab.getAttribute('data-file');
            SaveFile(filename, codearea.value);
        }
    } else if (e.ctrlKey && e.key === 'o') {
        const [contents, filename] = OpenFileDialog();
        const groups = /.*([\/\\](.*))$/.exec(filename);
        addTab(groups[2], contents);
        activeTab.setAttribute('data-file', filename);
    } else if (e.ctrlKey && e.key === 'n') {
        addTab('Untitled');
    } else if (e.ctrlKey && e.key === 'Enter') {
        e.preventDefault();
        OnRun(activeTab.getAttribute('data-file'));
        terminal.removeAttribute('readonly');
        terminalSection.classList.remove('hidden');
        terminal.focus();
        e.stopPropagation();
    } else if (e.ctrlKey && e.key === 'Tab') {
        // TODO: next tab
    } else if (e.ctrlKey && e.key === 'F4') {
        if (activeTab) {
            activeTab.children[2].click();
        }
    } else if (e.ctrlKey && e.key === '`') {
        toggleTerminal();
    }
}

let id = 1;
let activeTab;

function tabElement(name, content) {
    const template = document.createElement('template');
    const tabId = id++;

    template.innerHTML = `
        <div id="tab-${tabId}" class="bg-[#1e293b] tab border-r border-l border-t border-b-0 border-white/70 py-3 px-3 flex items-center active">
            <svg class="fill-white w-5 h-5" xmlns="http://www.w3.org/2000/svg" x="0px" y="0px" viewBox="0 0 115.28 122.88">
                <g><path style="fill-rule: evenodd; clip-rule: evenodd;" d="M25.38,57h64.88V37.34H69.59c-2.17,0-5.19-1.17-6.62-2.6c-1.43-1.43-2.3-4.01-2.3-6.17V7.64l0,0H8.15 c-0.18,0-0.32,0.09-0.41,0.18C7.59,7.92,7.55,8.05,7.55,8.24v106.45c0,0.14,0.09,0.32,0.18,0.41c0.09,0.14,0.28,0.18,0.41,0.18 c22.78,0,58.09,0,81.51,0c0.18,0,0.17-0.09,0.27-0.18c0.14-0.09,0.33-0.28,0.33-0.41v-11.16H25.38c-4.14,0-7.56-3.4-7.56-7.56 V64.55C17.82,60.4,21.22,57,25.38,57L25.38,57z M69.94,82.64l6.9,2.08c-0.46,1.93-1.19,3.55-2.19,4.84c-1,1.3-2.23,2.28-3.71,2.93 c-1.48,0.66-3.35,0.99-5.64,0.99c-2.77,0-5.03-0.4-6.79-1.2c-1.75-0.81-3.26-2.22-4.54-4.24c-1.27-2.02-1.91-4.62-1.91-7.77 c0-4.21,1.12-7.44,3.36-9.71c2.25-2.26,5.42-3.39,9.52-3.39c3.21,0,5.73,0.65,7.57,1.94c1.83,1.3,3.2,3.29,4.09,5.98l-6.93,1.53 c-0.24-0.77-0.5-1.33-0.76-1.69c-0.44-0.6-0.98-1.06-1.62-1.38c-0.64-0.33-1.35-0.49-2.14-0.49c-1.79,0-3.16,0.72-4.11,2.14 c-0.72,1.06-1.08,2.73-1.08,5c0,2.82,0.43,4.75,1.29,5.8c0.86,1.04,2.06,1.56,3.61,1.56c1.51,0,2.64-0.42,3.41-1.27 C69.03,85.48,69.59,84.25,69.94,82.64L69.94,82.64z M97.79,57h9.93c4.16,0,7.56,3.41,7.56,7.56v31.42c0,4.15-3.41,7.56-7.56,7.56 h-9.93v13.55c0,1.61-0.65,3.04-1.7,4.1c-1.06,1.06-2.49,1.7-4.1,1.7c-29.44,0-56.59,0-86.18,0c-1.61,0-3.04-0.64-4.1-1.7 c-1.06-1.06-1.7-2.49-1.7-4.1V5.85c0-1.61,0.65-3.04,1.7-4.1c1.06-1.06,2.53-1.7,4.1-1.7h58.72C64.66,0,64.8,0,64.94,0 c0.64,0,1.29,0.28,1.75,0.69h0.09c0.09,0.05,0.14,0.09,0.23,0.18l29.99,30.36c0.51,0.51,0.88,1.2,0.88,1.98 c0,0.23-0.05,0.41-0.09,0.65V57L97.79,57z M67.52,27.97V8.94l21.43,21.7H70.19c-0.74,0-1.38-0.32-1.89-0.78 C67.84,29.4,67.52,28.71,67.52,27.97L67.52,27.97z"/></g>
            </svg>
            <span class="ml-2">${name}</span>
            <button class="ml-2">X</button>
        </div>
    `;

    const [element] = template.content.children;
    element.setAttribute('data-content', content);
    element.onclick = (e) => {
        if (e.target instanceof HTMLButtonElement) {
            return;
        }
        activeTab.classList.remove('active');
        element.classList.add('active');
        activeTab = element;
        codearea.value = element.getAttribute('data-content');
    };

    const close = element.children[2];
    close.onclick = () => {
        let nextTab;
        for (let i = 0; i < tabs.children.length; i++) {
            if (tabs.children[i] === element) {
                if (i) {
                    nextTab = tabs.children[i - 1];
                } else if (tabs.children.length > 1) {
                    nextTab = tabs.children[i + 1];
                }
                break;
            }
        }
        element.remove();
        if (nextTab) {
            nextTab.click();
        } else {
            codearea.value = '';
        }
        if (tabs.children.length === 0) {
            codearea.setAttribute('readonly', 'readonly');
        }
    };

    return element;
}

function motion(e) {
    if (e.ctrlKey && e.key === 'Enter') {
        e.preventDefault();
        return;
    }

    if (e.shiftKey && e.key === 'Tab') {
        e.preventDefault();
        const start = codearea.selectionStart;
        const end = codearea.selectionEnd;
        const broke = codearea.value.substring(start, end);

        const spittingAVenom = broke.split('\n')
            .map(line => line.replace(/^\t/, ''));

        codearea.value = codearea.value.replace(broke, spittingAVenom.join('\n'));
    } else if (e.key === 'Tab') {
        e.preventDefault();
        const start = codearea.selectionStart;
        const end = codearea.selectionEnd;
        codearea.value = codearea.value.substring(0, start) + '\t' + codearea.value.substring(end);
        codearea.selectionStart = codearea.selectionEnd = start + 1;
    } else if (e.key === 'Enter') {
        let cursor = codearea.selectionStart;
        const before = codearea.value.substring(0, cursor).split('\n');
        const lastLine = before[before.length - 1];
        const indent = lastLine.match(/^\s+/);
        if (indent) {
            e.preventDefault();
            e.stopPropagation();
            codearea.value = codearea.value.substring(0, cursor) + '\n' + indent[0] + codearea.value.substring(cursor);
            cursor = codearea.selectionStart = codearea.selectionEnd = cursor + indent[0].length;
        }
    }

    if (e.defaultPrevented && e.key.length === 1) {
        activeTab.setAttribute('data-content', codearea.value);
    }
}

function addTab(label, content = '') {
    codearea.removeAttribute('readonly');
    const tab = tabElement(label, content);
    tabs.appendChild(tab);
    activeTab && activeTab.classList.remove('active');
    activeTab = tab;
    codearea.value = content;
    codearea.focus();
}

function toggleMenu(id) {
    const menu = document.getElementById(id);
    if (!menu.classList.toggle('hidden')) {
        menu.focus();
    }
}

let currentViewLayoutButton;

function toggleLayout(target) {
    currentViewLayoutButton && currentViewLayoutButton.toggleAttribute('active');
    currentViewLayoutButton = target;
    currentViewLayoutButton.toggleAttribute('active');
    const list = document.getElementById('textareas').classList;
    if (list.contains('flex-row')) {
        list.replace('flex-row', 'flex-col');
    } else {
        list.replace('flex-col', 'flex-row');
    }
    document.querySelectorAll('#textareas section')
        .forEach(section => section.classList.toggle('h-screen'));
}

function toggleTerminal() {
    if (terminalSection.classList.toggle('hidden')) {
        codearea.focus();
    } else {
        terminal.focus();
    }
}
