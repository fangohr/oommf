window.onload = function() {
    element = document.getElementsByClassName("ltx_page_navbar")[0];
    var openbar = document.createElement("button");
    openbar.innerHTML = "&#9776; ";
    element.insertAdjacentElement('beforebegin',openbar);
    openbar.addEventListener ("click", function() {
        element.style.width = "auto";
        this.style.top = "-100px";
        closebar.style.top = "0";
    });
    var closebar = document.createElement("button");
    closebar.innerHTML = "&#9776; ";
    element.insertAdjacentElement('afterbegin',closebar);
    closebar.addEventListener ("click", function() {
        element.style.width = "0";
        this.style.top = "-100px";
        openbar.style.top = "";
    });
    element.style.width = "0";
    closebar.style.top = "-100px";
};
