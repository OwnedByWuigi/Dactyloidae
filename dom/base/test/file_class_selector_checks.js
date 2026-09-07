function checkClassSelectors(parse, equal) {
  function check(root, selector, expected) {
    var all = root.querySelectorAll(selector);
    equal(all.length, expected.length, selector + " count");
    for (var i = 0; i < expected.length; i++) {
      equal(all[i].id, expected[i], selector + " order " + i);
    }
    equal(root.querySelector(selector), all.length ? all[0] : null,
          selector + " first match");
  }

  var doc = parse('<!doctype html><html><body>' +
    '<section id="root" class="toggle"><div id="a" class="toggle completed">' +
    '<span id="b" class="other toggle"></span></div>' +
    '<div id="c" class="TOGGLE"></div><button id="d" class="toggle\t extra"></button>' +
    '<svg xmlns="http://www.w3.org/2000/svg"><g id="svg" class="toggle"/></svg>' +
    '</section></body></html>', 'text/html');
  var root = doc.getElementById('root');
  for (var repeat = 0; repeat < 10; repeat++) {
    check(root, '.toggle', ['a', 'b', 'd', 'svg']);
    check(root, '*|*.toggle', ['a', 'b', 'd', 'svg']);
    check(root, '.t\\6f ggle', ['a', 'b', 'd', 'svg']);
    check(root, '.TOGGLE', ['c']);
    check(root, '.missing', []);
    check(root, '.toggle.completed', ['a']);
    check(root, 'button.toggle', ['d']);
    check(root, '.toggle:not(.completed)', ['b', 'd', 'svg']);
    check(root, '.toggle, .other', ['a', 'b', 'd', 'svg']);
  }
  check(doc, '.toggle', ['root', 'a', 'b', 'd', 'svg']);

  var snapshot = root.querySelectorAll('.toggle');
  doc.getElementById('a').className = '';
  root.removeChild(doc.getElementById('d'));
  doc.getElementById('c').className = 'toggle';
  check(root, '.toggle', ['b', 'c', 'svg']);
  equal(snapshot.length, 4, 'querySelectorAll remains a static snapshot');
  equal(snapshot[0].id, 'a', 'snapshot retains a changed element');
  equal(snapshot[2].id, 'd', 'snapshot retains a removed element');

  var fragment = doc.createDocumentFragment();
  fragment.appendChild(root);
  check(fragment, '.toggle', ['root', 'b', 'c', 'svg']);
  check(root, '.toggle', ['b', 'c', 'svg']);

  var quirks = parse('<html><body><div id="lower" class="toggle"></div>' +
                     '<div id="upper" class="TOGGLE"></div></body></html>', 'text/html');
  equal(quirks.compatMode, 'BackCompat', 'quirks document');
  check(quirks, '.toggle', ['lower', 'upper']);
  check(quirks, '.TOGGLE', ['lower', 'upper']);

  var xml = parse('<root><item id="plain" class="toggle"/>' +
                  '<item id="upper" class="TOGGLE"/>' +
                  '<item xmlns="urn:test" id="namespaced" class="toggle"/></root>',
                  'application/xml');
  check(xml, '.toggle', ['plain', 'namespaced']);
  check(xml, '|*.toggle', ['plain']);
  check(xml, '*|*.toggle', ['plain', 'namespaced']);
}
