document.addEventListener('keydown', function(event) {
    if (event.key === 'Enter' && !event.target.closest('input, textarea, select')) {
        window.location.href = "./html/home.html";
    }
});
