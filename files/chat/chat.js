const _id = id => document.getElementById(id);
function $(p,...args) {
  if (p===null) {
    if (args[0].constructor !== String) throw new Error('expected tag name');
    p = document.createElement(args.shift());
  }
  for (let x of args) {
    if (x.constructor === String) {
      p = p.appendChild( (p instanceof SVGElement || x==='svg')
        ? document.createElementNS('http://www.w3.org/2000/svg',x)
        : document.createElement(x)
      );
    } else if (x.constructor === Array) {
      x = x.filter(x=>!!x);
      if (x.length) p.classList.add(...x);
    } else if (x.constructor === Object) {
      for (const [key,val] of Object.entries(x)) {
        if (key==='style') {
          for (const [skey,sval] of Object.entries(val)) {
            if (sval!==null)
              p.style[skey] = sval;
            else
              p.style.removeProperty(skey);
          }
        } else {
          if (p instanceof SVGElement)
            p.setAttributeNS(null,key,val);
          else
            p.setAttribute(key,val);
        }
      }
    }
  }
  return p;
}
function clear(x) {
  for (let c; c = x.firstChild; ) x.removeChild(c);
  return x;
}
const last = xs => xs[xs.length-1];

document.addEventListener('DOMContentLoaded', () => {
  const in_name = _id('name');
  const in_text = _id('text');
  in_name.disabled = true;
  in_text.disabled = true;

  const ws = new WebSocket('ws'+window.location.origin.substr(4)+'/chat','chat');
  ws.onopen = function() {
    this.onmessage = function(e) {
      const [name,text] = e.data.split('\0');
      const msg = $(_id('chat'),'div',['msg']);
      $(msg,'div',['name']).textContent = name;
      const txt = $(msg,'div',['text']);
      for (const p of text.split('\n'))
        $(txt,'p').textContent = p;
    };

    in_text.addEventListener('keypress',function(e) {
      if (e.keyCode == 13 && e.shiftKey) {
        e.preventDefault();
        const name = in_name.value.trim();
        const text = in_text.value.trim();
        if (name.length===0) {
          in_name.focus();
          alert('Name must not be empty');
        } else if (text.length===0) {
          in_text.focus();
          alert('Text must not be empty');
        } else {
          in_name.value = name;
          in_text.value = '';
          ws.send(name+'\0'+text);
        }
      }
    });

    in_name.disabled = false;
    in_text.disabled = false;
    in_name.focus();
  };
});
