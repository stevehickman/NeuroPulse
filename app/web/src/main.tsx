import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './App.css';
import { initI18n, getCurrentLocale } from '../../../common/lib/i18n';
import { initNppsCore } from '../../../common/lib/nppsCore';

// The NPPS core (parse, serialize, validate, compile) is WebAssembly a browser must load before first use.
Promise.all([initI18n(), initNppsCore()]).then(() => {
  const locale = getCurrentLocale();
  document.documentElement.lang = locale.bcp47;
  document.documentElement.dir = locale.direction;

  ReactDOM.createRoot(document.getElementById('root')!).render(
    <React.StrictMode>
      <App />
    </React.StrictMode>
  );
});
