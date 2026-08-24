import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  build: {
    outDir: "../data/www",
    emptyOutDir: true,
    rollupOptions: {
      output: {
        manualChunks: undefined,
        entryFileNames: "app.js",
        chunkFileNames: "app.js",
        assetFileNames: (info) => {
          if (info.name?.endsWith(".css")) return "app.css";
          return "assets/[name][extname]";
        },
      },
    },
  },
  server: {
    proxy: {
      "/api": "http://10.0.0.1",
    },
  },
});
