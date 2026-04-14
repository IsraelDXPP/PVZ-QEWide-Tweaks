# CI/CD — GitHub Actions

Este directorio contiene el pipeline de integración y despliegue continuo del proyecto.

## Archivo: `build.yml`

### Triggers (cuándo se ejecuta)

| Evento | Descripción |
|--------|-------------|
| `push` a `main`/`master` | Build completo + release automático |
| `pull_request` a `main`/`master` | Build de verificación (sin release) |
| `workflow_dispatch` | Ejecución manual desde la pestaña Actions |

---

### Jobs

#### 1. `build` — Compila Debug y Release en paralelo

Ambas configuraciones corren simultáneamente en runners separados (`windows-latest`). Si uno falla, el otro sigue ejecutándose.

**Pasos:**

1. **Checkout** del repositorio completo (`fetch-depth: 0` para poder crear tags).
2. **Setup MSBuild** vía `microsoft/setup-msbuild@v2` — instala automáticamente las herramientas de Visual Studio disponibles en el runner.
3. **Cache de objetos intermedios** — cachea los directorios de build de MSBuild. La key incluye el hash de todos los `.cpp`, `.c`, `.h`, `.rc` y `.vcxproj`, invalidándose solo cuando cambia el código fuente.
4. **Build** con MSBuild:
   - `/m` — build en paralelo (usa todos los núcleos del runner)
   - `/p:UseMultiToolTask=true` — paralelización a nivel de compilación de archivos
   - Para **Debug**: sin optimizaciones, con símbolos completos (`EditAndContinue`), `_DEBUG`, checks de runtime, `MultiThreadedDebug`.
   - Para **Release**: optimización máxima (`/O2 + /GL`), `NDEBUG`, `WholeProgramOptimization`, `MultiThreaded`.
5. **Verificación** de que el `.exe` exista.
6. **Strip del binario Release** con `editbin /RELEASE` — actualiza el checksum PE y elimina paths de PDB embebidos.
7. **Empaquetado** — copia el `.exe`, `bass.dll` y todos los directorios de assets (`images/`, `sounds/`, `data/`, `reanim/`, etc.) a una carpeta intermedia. En Release elimina cualquier `.pdb`, `.ilk`, `.exp` o `.map` residual.
8. **Compresión**:
   - Release → `CompressionLevel: Optimal` (máxima compresión)
   - Debug → `CompressionLevel: Fastest` (más rápido, tamaño secundario)
9. **Upload** del `.zip` como artefacto descargable de la Action (retención: 30 días).

---

#### 2. `release` — Crea el GitHub Release (solo en push a main/master)

Corre en `ubuntu-latest` después de que ambos builds terminen exitosamente.

1. **Genera un tag** automático con formato `v2026.04.13-abc1234` (fecha + short SHA).
2. **Descarga** ambos `.zip` del job anterior.
3. **Crea** el tag y el GitHub Release con:
   - Nombre legible con el tag generado
   - Notas de release auto-generadas por GitHub (commits desde el release anterior)
   - Descripción con tabla comparativa Debug/Release
   - Ambos `.zip` adjuntos como assets descargables

---

### Contenido del paquete `.zip`

```
QEWide-Tweaks-Release.zip
├── LawnProject.exe      ← Ejecutable principal
├── bass.dll             ← Audio runtime
├── compiled/            ← Assets compiled (cache)
├── data/
├── images/
├── languages/
├── particles/
├── properties/
├── reanim/
├── resourcepacks/
└── sounds/
```

> **Nota:** El paquete Debug incluye los mismos archivos pero el `.exe` contiene símbolos de depuración completos y no está optimizado.

---

### Notas importantes

- **No rompe la compilación manual**: el workflow solo agrega archivos en `.github/workflows/` y no modifica ningún archivo de proyecto.
- **El runner `windows-latest`** incluye Visual Studio Build Tools con el toolset `v143` (VS 2022), que coincide con el `PlatformToolset` definido en el `.vcxproj`.
- **Los assets**: el workflow siempre los toma de la carpeta `Debug/` del repositorio, que es donde residen los assets versionados del juego. El ejecutable final vive en `Debug/` (Debug build) o `Release/` (Release build) según MSBuild.
- **Permisos**: el job `release` requiere `contents: write` en el token de GitHub Actions. Esto ya está configurado en el YAML.
