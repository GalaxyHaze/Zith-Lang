document.addEventListener('keydown', function(e) {
    if (e.key === 'Escape') {
        setTimeout(() => {
            window.location.href = "../home.html";
        }, 10);
    }
});