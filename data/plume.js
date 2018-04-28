(function () {
'use strict';

//Dom ready
var ready = function(fn)
{
  //Check for ready state
  if(document.readyState !== 'loading'){ return fn(null); }

  //Register the event listener
  document.addEventListener('DOMContentLoaded', function(event)
  {
    //Check the ready state
    if(document.readyState === 'loading'){ return; }

    //Call the provided function
    return fn(event);
  });
};

//Register the dom ready listener
ready(function()
{
  //Initialize the classnames list
  var classNames =
  {
    PARENT: 'plume',
    PARENT_JS: 'plume--js',
    PARENT_CLOSED: 'plume--closed',
    TOGGLE: 'plume-toggle'
  };

  //Get the class name
  var plume_class = '.' + classNames.PARENT + '.' + classNames.PARENT_JS;

  //Get the plume elements
  var plume_nodes = document.querySelectorAll(plume_class);

  //Check for no nodes
  if(plume_nodes.length === 0){ return; }

  //Check for more than 1 node
  if(plume_nodes.length > 1){ throw new Error('Only one plume class is allowed'); }

  //Save the parent element
  var parent = plume_nodes[0];

  //Find the navigation menu
  var nodes = parent.querySelectorAll('.' + classNames.TOGGLE);

  //Check the number of nodes
  if(nodes.length !== 1){ return null; }

  //Toggle menu function
  var toggle_menu = function()
  {
    //Add or remove the closed class to parent
    parent.classList.toggle(classNames.PARENT_CLOSED);

    //Set the local storage key
    window.localStorage.setItem('plume-closed', parent.classList.contains(classNames.PARENT_CLOSED));
  };

  //Register the mouse down event listener
  nodes[0].addEventListener('mousedown', function(event)
  {
    //Prevent default
    event.preventDefault();

    //Toggle the menu
    return toggle_menu();
  });

  //Register the touch start event listener
  nodes[0].addEventListener('touchstart', function(event)
  {
    //Stop event propagation
    event.stopPropagation();

    //Prevent default
    event.preventDefault();

    //Toggle the menu
    return toggle_menu();
  });

  //Check the local key
  if(window.localStorage.getItem('plume-closed') === 'true')
  {
    //Add the closed class
    parent.classList.add(classNames.PARENT_CLOSED);
  }
});

}());
