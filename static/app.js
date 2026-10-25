(() => {
  const dash = document.getElementById('dashboard');
  const btnAdd = document.getElementById('btn-add');
  const btnLogout = document.getElementById('btn-logout');
  const sessionInfo = document.getElementById('session-info');
  const wizard = document.getElementById('wizard');
  const wizBody = document.getElementById('wiz-body');
  const wizStepLabel = document.getElementById('wiz-step-label');
  const wizNext = document.getElementById('wiz-next');
  const wizBack = document.getElementById('wiz-back');
  const wizCancel = document.getElementById('wiz-cancel');

  let session = { auth: false, group: 'admin' };
  let types = [];
  let state = {};

  async function api(path, opts = {}) {
    const r = await fetch(path, {
      credentials: 'same-origin',
      headers: { 'Content-Type': 'application/json', ...(opts.headers || {}) },
      ...opts
    });
    if (r.status === 401) {
      location.href = '/login';
      throw new Error('unauthorized');
    }
    return r;
  }

  function renderView(card, view) {
    if (!view || !view.kind) return;
    card.dataset.kind = view.kind;
    let body = card.querySelector('.body');
    if (!body) {
      body = document.createElement('div');
      body.className = 'body';
      card.appendChild(body);
    }
    if (view.kind === 'toggle') {
      body.innerHTML = '';
      const sw = document.createElement('button');
      sw.className = 'switch' + (view.on ? ' on' : '');
      sw.title = view.on ? 'On' : 'Off';
      sw.addEventListener('click', async () => {
        const val = view.on ? 0 : 1;
        await api(`/widgets/${encodeURIComponent(view.id)}/action`, {
          method: 'POST',
          body: JSON.stringify({ val })
        });
        refreshOne(view.id);
      });
      body.appendChild(sw);
    } else if (view.kind === 'slider') {
      body.innerHTML = '';
      const row = document.createElement('div');
      const range = document.createElement('input');
      range.type = 'range';
      range.min = view.min ?? 0;
      range.max = view.max ?? 100;
      range.step = 0.1;
      range.value = view.value ?? 0;
      const lab = document.createElement('div');
      lab.className = 'meta';
      lab.textContent = String(view.value ?? 0);
      range.addEventListener('change', async () => {
        await api(`/widgets/${encodeURIComponent(view.id)}/action`, {
          method: 'POST',
          body: JSON.stringify({ val: Number(range.value) })
        });
        refreshOne(view.id);
      });
      range.addEventListener('input', () => { lab.textContent = range.value; });
      row.appendChild(range);
      row.appendChild(lab);
      body.appendChild(row);
    } else if (view.kind === 'sensor') {
      body.innerHTML = `<div class="sensor-value">${view.value ?? '—'}</div>`;
    } else if (view.kind === 'chart') {
      body.innerHTML = '';
      const canvas = document.createElement('canvas');
      canvas.className = 'chart';
      canvas.width = 400;
      canvas.height = 120;
      body.appendChild(canvas);
      const rows = view.rows || [];
      const ctx = canvas.getContext('2d');
      ctx.fillStyle = '#11141a';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      if (rows.length > 1) {
        const vals = rows.map(r => Number(r.v));
        const min = Math.min(...vals);
        const max = Math.max(...vals);
        const span = max - min || 1;
        ctx.strokeStyle = '#4c8bf5';
        ctx.beginPath();
        rows.forEach((r, i) => {
          const x = (i / (rows.length - 1)) * (canvas.width - 8) + 4;
          const y = canvas.height - 4 - ((Number(r.v) - min) / span) * (canvas.height - 8);
          if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
        });
        ctx.stroke();
      } else {
        ctx.fillStyle = '#9aa0a6';
        ctx.fillText('No data', 12, 24);
      }
    }
  }

  function ensureCard(w) {
    let card = dash.querySelector(`[data-id="${CSS.escape(w.id)}"]`);
    if (!card) {
      card = document.createElement('section');
      card.className = 'card';
      card.dataset.id = w.id;
      const h = document.createElement('h2');
      h.textContent = w.title || w.id;
      card.appendChild(h);
      const meta = document.createElement('div');
      meta.className = 'meta';
      meta.textContent = `${w.type} · ${w.entity?.source || ''}`;
      card.appendChild(meta);
      if (session.group === 'admin') {
        const del = document.createElement('button');
        del.className = 'secondary';
        del.textContent = 'Elimina';
        del.addEventListener('click', async () => {
          if (!confirm('Eliminare questo widget?')) return;
          await api(`/widgets/${encodeURIComponent(w.id)}`, { method: 'DELETE' });
          loadDashboard();
        });
        card.appendChild(del);
      }
      dash.appendChild(card);
    }
    const h = card.querySelector('h2');
    if (h) h.textContent = w.title || w.id;
    if (w.view) renderView(card, w.view);
    return card;
  }

  async function refreshOne(id) {
    const r = await api(`/widgets/${encodeURIComponent(id)}/data`);
    const view = await r.json();
    const card = dash.querySelector(`[data-id="${CSS.escape(id)}"]`);
    if (card) renderView(card, view);
  }

  async function loadDashboard() {
    const r = await api('/dashboard');
    const data = await r.json();
    const ids = new Set((data.widgets || []).map(w => w.id));
    [...dash.querySelectorAll('.card')].forEach(c => {
      if (!ids.has(c.dataset.id)) c.remove();
    });
    if (!data.widgets || data.widgets.length === 0) {
      dash.innerHTML = '<div class="empty">Nessun widget. Usa «Aggiungi widget» (admin).</div>';
      return;
    }
    if (dash.querySelector('.empty')) dash.innerHTML = '';
    data.widgets.forEach(ensureCard);
  }

  /* —— Wizard: io_kind → entity → type → fields —— */
  const wiz = {
    step: 0,
    io_kind: null,
    entity: null,
    type: null,
    fields: {}
  };

  function filterTypes() {
    const need = wiz.io_kind; // digital|analog
    const sourceTag = wiz.entity?.source === 'history' ? 'history' : 'live';
    return types.filter(t => {
      const tags = t.tags || [];
      return tags.includes(need) && tags.includes(sourceTag);
    });
  }

  async function loadEntities() {
    if (wiz.stepIntent === 'history') {
      const r = await api('/labels');
      if (!r.ok) return [];
      const labels = await r.json();
      return (labels || []).map(l => ({
        source: 'history',
        label: l.name,
        name: l.name,
        domain: '',
        title: l.description || l.name
      }));
    }
    const r = await api('/api/io');
    if (!r.ok) return [];
    const io = await r.json();
    const out = [];
    const dig = wiz.io_kind === 'digital';
    const add = (domain, obj) => {
      Object.keys(obj || {}).forEach(name => {
        const isDig = /^Dig/i.test(name) || (obj[name].unit === 'OnOff');
        const isAna = /^Ana/i.test(name) || !isDig;
        if ((dig && isDig) || (!dig && isAna))
          out.push({ source: 'bridge', domain, name, label: '', title: obj[name].description || name });
      });
    };
    add('input', io.inputs);
    add('output', io.outputs);
    return out;
  }

  async function renderWizard() {
    wizBack.hidden = wiz.step === 0;
    wizNext.textContent = wiz.step === 3 ? 'Salva' : 'Avanti';
    if (wiz.step === 0) {
      wizStepLabel.textContent = '1 — Tipo I/O';
      wizBody.innerHTML = `
        <div class="form-row"><label>I/O</label>
          <select id="w-io"><option value="digital">Digitale</option><option value="analog">Analogico</option></select>
        </div>
        <div class="form-row"><label>Fonte</label>
          <select id="w-src"><option value="bridge">Live (http-bridge)</option><option value="history">Storico (MySQL)</option></select>
        </div>`;
    } else if (wiz.step === 1) {
      wizStepLabel.textContent = '2 — Entità';
      const ents = await loadEntities();
      wizBody.innerHTML = `<div class="form-row"><label>Entità</label>
        <select id="w-ent">${ents.map((e,i)=>`<option value="${i}">${e.title} (${e.source})</option>`).join('')}</select>
      </div>`;
      wiz._ents = ents;
    } else if (wiz.step === 2) {
      wizStepLabel.textContent = '3 — Widget';
      const ft = filterTypes();
      wizBody.innerHTML = `<div class="form-row"><label>Tipo widget</label>
        <select id="w-type">${ft.map(t=>`<option value="${t.type}">${t.label}</option>`).join('')}</select>
      </div>`;
      wiz._ft = ft;
    } else if (wiz.step === 3) {
      wizStepLabel.textContent = '4 — Configurazione';
      const t = types.find(x => x.type === wiz.type) || {};
      const fields = t.fields || [];
      wizBody.innerHTML = `
        <div class="form-row"><label>ID</label><input id="w-id" value="${(wiz.entity?.name || 'widget').toLowerCase().replace(/[^a-z0-9]+/g,'-')}"/></div>
        <div class="form-row"><label>Titolo</label><input id="w-title" value="${wiz.entity?.title || ''}"/></div>
        ${fields.filter(f => f.name !== 'title').map(f => `
          <div class="form-row"><label>${f.label || f.name}</label>
            <input data-field="${f.name}" value="${f.default ?? ''}"/>
          </div>`).join('')}`;
    }
  }

  async function wizAdvance() {
    if (wiz.step === 0) {
      wiz.io_kind = document.getElementById('w-io').value;
      wiz.stepIntent = document.getElementById('w-src').value;
      wiz.step = 1;
    } else if (wiz.step === 1) {
      const i = Number(document.getElementById('w-ent').value);
      wiz.entity = wiz._ents[i];
      if (!wiz.entity) return;
      wiz.step = 2;
    } else if (wiz.step === 2) {
      wiz.type = document.getElementById('w-type').value;
      wiz.step = 3;
    } else if (wiz.step === 3) {
      const id = document.getElementById('w-id').value.trim();
      const title = document.getElementById('w-title').value.trim();
      const fields = { title };
      wizBody.querySelectorAll('[data-field]').forEach(el => {
        fields[el.dataset.field] = el.value;
      });
      const payload = {
        id,
        type: wiz.type,
        title,
        io_kind: wiz.io_kind,
        entity: {
          source: wiz.entity.source,
          domain: wiz.entity.domain || '',
          name: wiz.entity.name || '',
          label: wiz.entity.label || wiz.entity.name || ''
        },
        fields
      };
      const r = await api('/widgets', { method: 'POST', body: JSON.stringify(payload) });
      if (!r.ok) {
        alert('Salvataggio fallito');
        return;
      }
      wizard.close();
      await loadDashboard();
      return;
    }
    await renderWizard();
  }

  btnAdd.addEventListener('click', async () => {
    wiz.step = 0;
    await renderWizard();
    wizard.showModal();
  });
  wizCancel.addEventListener('click', () => wizard.close());
  wizBack.addEventListener('click', async () => {
    if (wiz.step > 0) wiz.step--;
    await renderWizard();
  });
  wizNext.addEventListener('click', wizAdvance);

  btnLogout.addEventListener('click', async () => {
    await api('/logout', { method: 'POST' });
    location.href = '/login';
  });

  async function boot() {
    const sr = await fetch('/session', { credentials: 'same-origin' });
    if (sr.status === 401) {
      location.href = '/login';
      return;
    }
    session = await sr.json();
    sessionInfo.textContent = session.auth
      ? `${session.user} (${session.group})`
      : 'auth off';
    if (session.group === 'admin' || session.auth === false)
      btnAdd.hidden = false;
    if (session.auth)
      btnLogout.hidden = false;

    const tr = await api('/widget-types');
    types = await tr.json();
    await loadDashboard();
    setInterval(loadDashboard, 5000);
  }

  boot().catch(console.error);
})();
