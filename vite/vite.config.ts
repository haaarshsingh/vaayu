import { defineConfig } from 'vite'
import { resolve } from 'path'
import { readdirSync, statSync } from 'fs'

function findAssetEntries(dir: string, base = ''): Record<string, string> {
  const entries: Record<string, string> = {}
  
  try {
    const files = readdirSync(dir)
    for (const file of files) {
      const fullPath = resolve(dir, file)
      const relativePath = base ? `${base}/${file}` : file
      const stat = statSync(fullPath)
      
      if (stat.isDirectory()) {
        Object.assign(entries, findAssetEntries(fullPath, relativePath))
      } else if (file.endsWith('.ts') || file.endsWith('.js') || file.endsWith('.css')) {
        const name = relativePath.replace(/\.(ts|js|css)$/, '')
        entries[name] = fullPath
      }
    }
  } catch {
    // Directory doesn't exist yet
  }
  
  return entries
}

const siteDir = resolve(__dirname, '../site')
const assetEntries = findAssetEntries(siteDir)

export default defineConfig({
  root: resolve(__dirname, '../site'),
  base: '/',
  build: {
    outDir: resolve(__dirname, '../dist'),
    emptyOutDir: false,
    manifest: true,
    assetsDir: 'assets',
    rollupOptions: {
      input: Object.keys(assetEntries).length > 0 ? assetEntries : undefined,
    },
    minify: 'esbuild', // Minify JS/CSS
    cssMinify: true,
  },
  server: {
    port: 5173,
    strictPort: true,
    cors: true,
  },
})

