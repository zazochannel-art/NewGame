# Island preview

`island-preview.html` is a browser preview of `ALIIslandGenerator`. It's a JavaScript port of the same algorithm: `FRandomStream` (bit-exact), `HashCombine`, classic Perlin, the same parameters, biomes and placement rules.

Use it to try seeds and tune parameters quickly without starting Unreal.
If you change a parameter in `LIIslandGenerator.h` / `.cpp`, update the `P` and `RULES` constants in the HTML file too.

Note: Unreal's `FMath::PerlinNoise2D` may use slightly different gradients, so the shape and proportions match, but exact positions can differ from the game.
