function sendText(form) {
    const xhr = new XMLHttpRequest();

    const fd = new FormData(form);

    xhr.open("POST", "/text");

    xhr.send(fd);
}

function onLoad() {
    const form = document.getElementById("text-form");

    form.addEventListener("submit", function(event) {
        event.preventDefault();

        sendText(form);
    })
}

function pacman() {
    const xhr = new XMLHttpRequest();

    const fd = new FormData();

    fd.append("animmode", String.fromCharCode(64));

    xhr.open("POST", "/anim");

    xhr.send(fd);
}

function wipe(dir) {
    const xhr = new XMLHttpRequest();

    const fd = new FormData();

    fd.append("animmode", String.fromCharCode(33));
    fd.append("dir", dir);

    xhr.open("POST", "/anim");

    xhr.send(fd);
}

function wipeDiagonal(dir) {
    const xhr = new XMLHttpRequest();

    const fd = new FormData();

    fd.append("animmode", String.fromCharCode(34));
    fd.append("dir", dir);

    xhr.open("POST", "/anim");

    xhr.send(fd);
}

function boxOutline(layer) {
    const xhr = new XMLHttpRequest();

    const fd = new FormData();

    fd.append("animmode", String.fromCharCode(36));
    fd.append("layer", layer);

    xhr.open("POST", "/anim");

    xhr.send(fd);
}