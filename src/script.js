'use strict';

document.addEventListener('DOMContentLoaded', () => {
    const commandInput = document.getElementById('command');

    commandInput.addEventListener('keydown', (event) => {
        if (event.key === 'Enter') {
            event.preventDefault();

            const command = commandInput.value;
            if (!command) {
                alert('Please enter a command.');
                return;
            }

            getBody(command);
            commandInput.value = '';
        }
    });
});

async function getBody(command) {
    try {
        const response = await fetch('/', {
            method: 'POST',
            headers: {
              "Content-type": "application/x-www-form-urlencoded; charset=UTF-8",
            },
            body: `command=${command}`,
          });

        if (!response.ok) {
            throw new Error('Network response was not ok');
        }

        const body = await response.text();
        console.log("Response received:");
        console.log(body);

        document.getElementById('output').textContent = body;
    } catch (error) {
        console.error('Error:', error);
    }
}