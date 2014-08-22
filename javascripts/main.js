window.onload = init;
 
function init()
{
	var config = document.getElementById('config');
	var qtpass = document.getElementById('qtpass');

    qtpass.onclick = function()
    {
        
        if (config.classList.contains('hidden')) {
        	config.classList.remove('hidden');
        } else {
        	config.classList.add('hidden');
        }
    }
    
    config.onclick = function()
    {
    	config.classList.add('hidden');
    }
}
 