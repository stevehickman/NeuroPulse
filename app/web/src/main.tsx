import React from 'react';
import ReactDOM from 'react-dom/client';
import './App.css';
import { initI18n, getCurrentLocale } from '../../../common/lib/i18n';
import { initNppsCore } from '../../../common/lib/nppsCore';

// The NPPS core (parse, serialize, validate, compile) is WebAssembly a browser must load before first use.
// The app is imported only once it is loaded: its modules call the core while they are evaluated (the
// protocol library starts loading the predefined set when it is constructed, and App's default limits are
// resolved by the core), and a static import would evaluate them before this line runs (REQ-NPPS-34 (a)).
Promise.all([initI18n(), initNppsCore()])
  .then(() => import('./App'))
  .then(({ default: App }) => {
    const locale = getCurrentLocale();
    document.documentElement.lang = locale.bcp47;
    document.documentElement.dir = locale.direction;

    ReactDOM.createRoot(document.getElementById('root')!).render(
      <React.StrictMode>
        <App />
      </React.StrictMode>
    );
  });
