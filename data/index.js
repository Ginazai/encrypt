document.getElementById("btnControl").addEventListener("change", function() {
    event.preventDefault();
    let statusText = document.getElementById('statusText');   
    if (this.checked) {  
        statusText.textContent = 'Encendido'; 
        statusText.style.color = 'green';
    } else { 
        statusText.textContent = 'Apagado';  
        statusText.style.color = 'red';  
    }
    statusText.style.opacity = '1';  
    let estado = this.checked ? 'on' : 'off'; // Determina el estado del LED
    // Enviar la solicitud usando fetch  
    fetch('/led?led_status=' + estado, { method: 'GET' })
    .then(response => response.text())   
    .catch(error => console.error('Error:', error)); 
});  
document.getElementById("options").addEventListener("click", () => {
    var dropdownContent = document.getElementsByClassName("dropdown-content");
    var icon = document.getElementById("dropdown-icon");
    dropdownContent = Array.from(dropdownContent);

    dropdownContent.map((dropItem) => {
        var aTag = dropItem.children;
        aTag = Array.from(aTag);

        if(dropItem.style.display == "none"){
            dropItem.style.display = "block";
            icon.src = 'arrow-down-white.png';
            aTag.map((a) => {
                a.style.display = "block";
            });
        } else {
            dropItem.style.display = "none";
            
            icon.src = 'arrow-left-white.png';
            aTag.map((a) => {
                a.style.display = "none";
            });
        }
    });
});