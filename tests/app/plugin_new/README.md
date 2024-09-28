

> 这个 plugin_new 插件的编译是独立的, 需要这样单独编译 `mkdir build && cd build && cmake .. && make -j4 && cd ..`
>
> 之后将 `build/bin/plugin/libplugin_new.so` 拷贝到对应目录中, 使用其他程序加载使用的.
>