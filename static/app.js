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
  let openMenu = null;

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

  function closeMenus() {
    if (openMenu) {
      openMenu.remove();
      openMenu = null;
    }
  }

  document.addEventListener('click', (e) => {
    if (openMenu && !openMenu.contains(e.target) && !e.target.closest('.card-menu-btn'))
      closeMenus();
  });

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
      range.step = view.step ?? 0.1;
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
      const head = document.createElement('div');
      head.className = 'card-head';
      const h = document.createElement('h2');
      h.textContent = w.title || w.id;
      head.appendChild(h);
      if (session.group === 'admin' || session.auth === false) {
        const menuBtn = document.createElement('button');
        menuBtn.type = 'button';
        menuBtn.className = 'card-menu-btn';
        menuBtn.setAttribute('aria-label', 'Menu');
        menuBtn.textContent = '⋮';
        menuBtn.addEventListener('click', (ev) => {
          ev.stopPropagation();
          closeMenus();
          const menu = document.createElement('div');
          menu.className = 'card-menu';
          const edit = document.createElement('button');
          edit.type = 'button';
          edit.textContent = 'Modifica';
          edit.addEventListener('click', () => {
            closeMenus();
            openEditWizard(w);
          });
          const del = document.createElement('button');
          del.type = 'button';
          del.className = 'danger-text';
          del.textContent = 'Elimina';
          del.addEventListener('click', async () => {
            closeMenus();
            if (!confirm('Eliminare questo widget?')) return;
            await api(`/widgets/${encodeURIComponent(w.id)}`, { method: 'DELETE' });
            loadDashboard();
          });
          menu.appendChild(edit);
          menu.appendChild(del);
          card.appendChild(menu);
          openMenu = menu;
        });
        head.appendChild(menuBtn);
      }
      card.appendChild(head);
      const meta = document.createElement('div');
      meta.className = 'meta';
      meta.textContent = `${w.type} · ${w.entity?.source || ''}`;
      card.appendChild(meta);
      dash.appendChild(card);
    }
    const h = card.querySelector('h2');
    if (h) h.textContent = w.title || w.id;
    const meta = card.querySelector('.meta');
    if (meta)
      meta.textContent = `${w.type} · ${w.entity?.source || ''}`;
    card._widget = w;
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

  /* —— Wizard: io → entity → type → [feedback] → fields —— */
  const wiz = {
    step: 0,
    editId: null,
    io_kind: null,
    stepIntent: null,
    entity: null,
    feedback: null,
    type: null,
    fields: {}
  };

  function usesFeedbackStep() {
    return wiz.stepIntent === 'bridge';
  }

  function configStep() {
    return usesFeedbackStep() ? 4 : 3;
  }

  function filterTypes() {
    const need = wiz.io_kind;
    const sourceTag = wiz.entity?.source === 'history' ? 'history' : 'live';
    return types.filter(t => {
      const tags = t.tags || [];
      return tags.includes(need) && tags.includes(sourceTag);
    });
  }

  function filterIoMap(obj, dig) {
    const out = [];
    Object.keys(obj || {}).forEach(name => {
      const isDig = /^Dig/i.test(name) || (obj[name].unit === 'OnOff');
      const isAna = /^Ana/i.test(name) || !isDig;
      if ((dig && isDig) || (!dig && isAna))
        out.push({
          source: 'bridge',
          domain: '',
          name,
          label: '',
          title: obj[name].description || name
        });
    });
    return out;
  }

  async function loadCommandEntities() {
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
    const dig = wiz.io_kind === 'digital';
    const list = filterIoMap(io.inputs, dig);
    list.forEach(e => { e.domain = 'input'; });
    return list;
  }

  async function loadFeedbackEntities() {
    const r = await api('/api/io');
    if (!r.ok) return [];
    const io = await r.json();
    const dig = wiz.io_kind === 'digital';
    const list = filterIoMap(io.outputs, dig);
    list.forEach(e => { e.domain = 'output'; });
    return list;
  }

  function resetWizard() {
    wiz.step = 0;
    wiz.editId = null;
    wiz.io_kind = null;
    wiz.stepIntent = null;
    wiz.entity = null;
    wiz.feedback = null;
    wiz.type = null;
    wiz.fields = {};
    wiz._ents = null;
    wiz._fbs = null;
    wiz._ft = null;
  }

  async function openEditWizard(w) {
    resetWizard();
    wiz.editId = w.id;
    wiz.io_kind = w.io_kind || 'digital';
    wiz.stepIntent = w.entity?.source === 'history' ? 'history' : 'bridge';
    wiz.entity = {
      source: w.entity?.source || 'bridge',
      domain: w.entity?.domain || '',
      name: w.entity?.name || '',
      label: w.entity?.label || '',
      title: w.title || w.entity?.name || w.id
    };
    if (w.feedback?.name) {
      wiz.feedback = {
        source: w.feedback.source || 'bridge',
        domain: w.feedback.domain || 'output',
        name: w.feedback.name,
        label: w.feedback.label || '',
        title: w.feedback.label || w.feedback.name
      };
    } else {
      wiz.feedback = null;
    }
    wiz.type = w.type;
    wiz.fields = { ...(w.fields || {}) };
    wiz.step = configStep();
    await renderWizard();
    wizard.showModal();
  }

  async function renderWizard() {
    wizBack.hidden = wiz.step === 0;
    const last = configStep();
    wizNext.textContent = wiz.step === last ? 'Salva' : 'Avanti';

    if (wiz.step === 0) {
      wizStepLabel.textContent = '1 — Tipo I/O';
      wizBody.innerHTML = `
        <div class="form-row"><label>I/O</label>
          <select id="w-io">
            <option value="digital"${wiz.io_kind === 'digital' ? ' selected' : ''}>Digitale</option>
            <option value="analog"${wiz.io_kind === 'analog' ? ' selected' : ''}>Analogico</option>
          </select>
        </div>
        <div class="form-row"><label>Fonte</label>
          <select id="w-src">
            <option value="bridge"${wiz.stepIntent !== 'history' ? ' selected' : ''}>Live (http-bridge)</option>
            <option value="history"${wiz.stepIntent === 'history' ? ' selected' : ''}>Storico (MySQL)</option>
          </select>
        </div>`;
    } else if (wiz.step === 1) {
      wizStepLabel.textContent = '2 — Entità comando';
      const ents = await loadCommandEntities();
      wiz._ents = ents;
      const sel = Math.max(0, ents.findIndex(e => e.name === wiz.entity?.name && e.domain === (wiz.entity?.domain || e.domain)));
      wizBody.innerHTML = `<div class="form-row"><label>Entità (scrittura)</label>
        <select id="w-ent">${ents.map((e, i) =>
          `<option value="${i}"${i === sel ? ' selected' : ''}>${e.name} — ${e.title}</option>`).join('')}
        </select>
      </div>`;
    } else if (wiz.step === 2) {
      wizStepLabel.textContent = '3 — Widget';
      const ft = filterTypes();
      wiz._ft = ft;
      wizBody.innerHTML = `<div class="form-row"><label>Tipo widget</label>
        <select id="w-type">${ft.map(t =>
          `<option value="${t.type}"${t.type === wiz.type ? ' selected' : ''}>${t.label}</option>`).join('')}
        </select>
      </div>`;
    } else if (wiz.step === 3 && usesFeedbackStep()) {
      wizStepLabel.textContent = '4 — Feedback';
      const fbs = await loadFeedbackEntities();
      wiz._fbs = fbs;
      const cur = wiz.feedback?.name || '';
      wizBody.innerHTML = `<div class="form-row"><label>Output feedback (stato)</label>
        <select id="w-fb">
          <option value="">Nessuno (usa entità comando)</option>
          ${fbs.map((e, i) =>
            `<option value="${i}"${e.name === cur ? ' selected' : ''}>${e.name} — ${e.title}</option>`).join('')}
        </select>
      </div>
      <p class="meta">Scegli un DigOut/AnaOut mirato dalle rules verso l'impianto.</p>`;
    } else if (wiz.step === last) {
      const n = usesFeedbackStep() ? 5 : 4;
      wizStepLabel.textContent = `${n} — Configurazione`;
      const t = types.find(x => x.type === wiz.type) || {};
      const fields = t.fields || [];
      const idVal = wiz.editId || (wiz.entity?.name || 'widget').toLowerCase().replace(/[^a-z0-9]+/g, '-');
      const titleVal = wiz.fields.title || wiz.entity?.title || '';
      wizBody.innerHTML = `
        <div class="form-row"><label>ID</label>
          <input id="w-id" value="${idVal}" ${wiz.editId ? 'readonly' : ''}/>
        </div>
        <div class="form-row"><label>Titolo</label>
          <input id="w-title" value="${titleVal}"/>
        </div>
        ${fields.filter(f => f.name !== 'title').map(f => `
          <div class="form-row"><label>${f.label || f.name}</label>
            <input data-field="${f.name}" value="${wiz.fields[f.name] ?? f.default ?? ''}"/>
          </div>`).join('')}`;
    }
  }

  function buildPayload(id, title, fields) {
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
    if (wiz.feedback?.name) {
      payload.feedback = {
        source: wiz.feedback.source || 'bridge',
        domain: wiz.feedback.domain || 'output',
        name: wiz.feedback.name,
        label: wiz.feedback.label || wiz.feedback.name
      };
    } else {
      payload.feedback = { source: '', domain: '', name: '', label: '' };
    }
    return payload;
  }

  async function wizAdvance() {
    const last = configStep();
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
      wiz.step = usesFeedbackStep() ? 3 : last;
    } else if (wiz.step === 3 && usesFeedbackStep()) {
      const v = document.getElementById('w-fb').value;
      if (v === '')
        wiz.feedback = null;
      else
        wiz.feedback = wiz._fbs[Number(v)];
      wiz.step = last;
    } else if (wiz.step === last) {
      const id = document.getElementById('w-id').value.trim();
      const title = document.getElementById('w-title').value.trim();
      const fields = { title };
      wizBody.querySelectorAll('[data-field]').forEach(el => {
        fields[el.dataset.field] = el.value;
      });
      if (!id || !wiz.type || !wiz.entity) {
        alert('Dati incompleti');
        return;
      }
      const payload = buildPayload(id, title, fields);
      let r;
      if (wiz.editId) {
        r = await api(`/widgets/${encodeURIComponent(wiz.editId)}`, {
          method: 'PUT',
          body: JSON.stringify(payload)
        });
      } else {
        r = await api('/widgets', { method: 'POST', body: JSON.stringify(payload) });
      }
      if (!r.ok) {
        alert('Salvataggio fallito');
        return;
      }
      wizard.close();
      resetWizard();
      await loadDashboard();
      return;
    }
    await renderWizard();
  }

  btnAdd.addEventListener('click', async () => {
    resetWizard();
    await renderWizard();
    wizard.showModal();
  });
  wizCancel.addEventListener('click', () => {
    wizard.close();
    resetWizard();
  });
  wizBack.addEventListener('click', async () => {
    const last = configStep();
    if (wiz.step === last && usesFeedbackStep())
      wiz.step = 3;
    else if (wiz.step === last)
      wiz.step = 2;
    else if (wiz.step > 0)
      wiz.step--;
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
