function onLoad() {
    const form = document.getElementById("text-form");

    form.addEventListener("submit", function(event) {
        event.preventDefault();

        sendText(form);
    })
}

function sendUpdate(fd) {
    const xhr = new XMLHttpRequest();

    xhr.open("POST", "/update");

    xhr.send(fd);
}

function off() {
    const fd = new FormData();
    fd.append("mode", "0");
    sendUpdate(fd);
}

function sendText(form) {
    const fd = new FormData(form);
    fd.append("mode", "1");

    sendUpdate(fd);
}

function pacman() {
    const fd = new FormData();
    fd.append("mode", "2");
    fd.append("animmode", "5");
    sendUpdate(fd);
}

function wipe(dir) {
    const fd = new FormData();
    fd.append("mode", "2");
    fd.append("animmode", "1");
    fd.append("dir", dir);
    sendUpdate(fd);
}

function wipeDiagonal(dirH, dirV) {
    const fd = new FormData();
    fd.append("mode", "2");
    fd.append("animmode", "2");
    fd.append("dirH", dirH);
    fd.append("dirV", dirV);
    sendUpdate(fd);
}

function boxOutline(layer) {
    const fd = new FormData();
    fd.append("mode", "2");
    fd.append("animmode", "4");
    fd.append("layer", layer);
    sendUpdate(fd);
}

function datetime() {
    const fd = new FormData();
    fd.append("mode", "3");
    sendUpdate(fd);
}
