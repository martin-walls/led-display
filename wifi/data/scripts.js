function sendText() {
    var text = document.getElementById("text-input").value;

    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/text", true);
    xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
    xhr.send(text);
}