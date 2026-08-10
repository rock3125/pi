/**
 * Which ground the instrument is drawn on.  Light is the default and the OS
 * preference is deliberately not consulted: `prefers-color-scheme` says what
 * the machine is set to, not whether this window is the one sitting next to a
 * terminal at night.  The choice is the toggle in the top rail, and it sticks.
 *
 * The same key is read by the inline script in index.html, which applies a
 * stored choice before the first paint.
 */

export type Theme = 'light' | 'dark'

const KEY = 'piview:theme'

export function storedTheme(): Theme {
  try {
    return localStorage.getItem(KEY) === 'dark' ? 'dark' : 'light'
  } catch {
    // private mode, or storage refused
    return 'light'
  }
}

export function applyTheme(theme: Theme): void {
  document.documentElement.dataset.theme = theme
  try {
    localStorage.setItem(KEY, theme)
  } catch {
    // the surface still changes; only the memory of it is lost
  }
}
