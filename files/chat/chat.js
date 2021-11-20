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
  const ws = new WebSocket('ws'+window.location.origin.substr(4)+'/chat','chat');
  ws.onopen = function() {
    this.send('Message from JavaScript.');
    this.onmessage = function(e) {
      const msg = $(_id('chat'),'div',['msg']);
      $(msg,'div',['name']).textContent = 'Anonymous';
      const txt = $(msg,'div',['text']);
      for (const p of e.data.split('\n'))
        $(txt,'p').textContent = p;
    };
  };
});
