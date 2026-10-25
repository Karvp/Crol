# Crol

When I was younger, I loved the idea of writing an app, sending it to someone, and having it just work. But I mostly knew Python and other interpreted languages, which made me curious: **how do interpreters actually work?**

Then I found Robert Nystrom's [*Crafting Interpreters*](https://craftinginterpreters.com/). I learned from the book, experimented on my own, and ended up creating **ROL**, a small programming language, with **Crol** as its C++ bytecode interpreter.

I didn't really have a practical goal. I wanted to learn and satisfy my curiosity. Building a language myself taught me much more than I expected.

## Build (Windows)

1. Install **Visual Studio 2022** with the **Desktop development with C++** workload.
2. Open `Crol.sln`, choose **Release** and **x64**, then select **Build → Build Solution**.
3. The executable will be at `x64/Release/Crol.exe`.

Alternatively, in a Visual Studio Developer PowerShell:

~~~powershell
msbuild Crol.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /p:UseMultiToolTask=false /p:CL_MPCount=1
~~~

## License

GPL-3.0-only. See [LICENSE](LICENSE). Third-party dependencies and materials remain under their respective licenses.
