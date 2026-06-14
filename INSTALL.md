# Physics Asset Body Tool — установка для Unreal Engine 5.5

Плагин рассчитан на Unreal Engine 5.5 и является Editor-only C++ плагином.

## Рекомендуемый способ: установка в проект

Используйте установку в проект, если плагин нужен только одному проекту или вы хотите держать его версию вместе с исходниками проекта.

1. Закройте Unreal Editor.
2. Скопируйте папку `Plugins/PhysicsAssetBodyTool` в корень вашего проекта:

   ```text
   YourProject/
   ├── YourProject.uproject
   └── Plugins/
       └── PhysicsAssetBodyTool/
           └── PhysicsAssetBodyTool.uplugin
   ```

3. Если в проекте нет папки `Plugins`, создайте её.
4. Откройте `.uproject` файл или сгенерируйте project files для IDE.
5. Соберите проект под Unreal Engine 5.5.
6. Откройте Unreal Editor и включите плагин в `Edit > Plugins`, если он не включился автоматически.
7. Окно инструмента доступно через `Window > Physics Asset Body Tool`.

## Альтернатива: установка в движок

Установка в папку движка подходит, если плагин должен быть доступен всем проектам этой локальной установки Unreal Engine 5.5.

1. Закройте Unreal Editor.
2. Скопируйте папку `PhysicsAssetBodyTool` в:

   ```text
   <UE_5.5>/Engine/Plugins/Marketplace/PhysicsAssetBodyTool/
   ```

   или в пользовательскую категорию, например:

   ```text
   <UE_5.5>/Engine/Plugins/Editor/PhysicsAssetBodyTool/
   ```

3. Пересоберите движок/проект, если используется source build или C++ проект.
4. Запустите Unreal Editor 5.5 и включите плагин в `Edit > Plugins`.
5. Откройте окно через `Window > Physics Asset Body Tool`.

## Что выбрать

- **В проект** — рекомендуется для разработки, командной работы и версионирования вместе с игрой.
- **В движок** — удобно, если один и тот же плагин нужен многим проектам на одной машине, но обновления сложнее контролировать.

## Важное замечание для Blueprint-only проектов

Это C++ Editor plugin. Если проект был Blueprint-only, Unreal может попросить скомпилировать C++ модуль. В таком случае установите Visual Studio/Rider с C++ toolchain, сгенерируйте project files и выполните сборку проекта под UE 5.5.
