document.addEventListener('DOMContentLoaded', function () {

  // Get all "navbar-burger" elements
  var $navbarBurgers = Array.prototype.slice.call(document.querySelectorAll('.navbar-burger'), 0);

  // Check if there are any navbar burgers
  if ($navbarBurgers.length > 0)
  {
    // Add a click event on each of them
    $navbarBurgers.forEach(function ($el) {
      $el.addEventListener('click', function () {

        // Get the target from the "data-target" attribute
        var target = $el.dataset.target;
        var $target = document.getElementById(target);

        // Toggle the class on both the "navbar-burger" and the "navbar-menu"
        $el.classList.toggle('is-active');
        $target.classList.toggle('is-active');

      });
    });
  }
});

function showDisconnected() {
	// sbox = document.getElementById('status_box');
	// sbox.className = "tile is-child notification is-danger is-marginless";
	
	var sbox = document.getElementById('modalDisconnected');
	sbox.className = "modal is-active";
	ws.close();
	
	
	console.log("show DISCONNECTED");
}
function hideDisconnected() {
	var sbox = document.getElementById('status_box');
	sbox.className = "tile is-child notification is-danger is-marginless is-hidden";
	console.log("hide DISCONNECTED");
}