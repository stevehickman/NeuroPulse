import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './App.css';
import { initI18n, getCurrentLocale } from './lib/i18n';

initI18n().then(() => {
  const locale = getCurrentLocale();
  document.documentElement.lang = locale.bcp47;
  document.documentElement.dir = locale.direction;

  ReactDOM.createRoot(document.getElementById('root')!).render(
    <React.StrictMode>
      <App />
    </React.StrictMode>
  );
});
