<!--

function displayBody(bodyId) {
   var bodies = document.getElementsByClassName("w3-main");
   var i;
   for(i = 0; i < bodies.length; i++) {
      bodies[i].style.display = (bodies[i].id == bodyId) ? "block" : "none"; 
   }
}

function w3_open() {
    if (mySidebar.style.display === 'block') {
        mySidebar.style.display = 'none';
        overlayBg.style.display = "none";
    } else {
        mySidebar.style.display = 'block';
        overlayBg.style.display = "block";
    }
}

function w3_close() {
    mySidebar.style.display = "none";
    overlayBg.style.display = "none";
}

function op(complete_function) {
  var x = new XMLHttpRequest();
  var u = "ajax";
  var i;
  for(i = 1; i < arguments.length; i++) {
    if(i === 1)
      u += "?";
    else
       u += "&";
    u += arguments[i];
  }
  if(complete_function)
  {
    x.onreadystatechange = function() {
      if (x.readyState == 4) {
        complete_function(x.responseText);
      }
    }
  }
  x.open("GET", u, true);
  x.send();
}

function getValue(parameterName, valueDefault, callback) {
    var valueResult = valueDefault;
    $.ajax({url: "ajax?"+parameterName, async : false, success: function(result){
      valueResult = result; 
    }});
    return valueResult;
}

-->
