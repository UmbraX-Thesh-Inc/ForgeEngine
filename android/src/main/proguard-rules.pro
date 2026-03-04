# ForgeEngine ProGuard rules

# Keep all JMonkeyEngine classes
-keep class com.jme3.** { *; }
-dontwarn com.jme3.**

# Keep all ForgeEngine classes (JNI accessed)
-keep class com.forgeengine.** { *; }
-keepclassmembers class com.forgeengine.** { *; }

# Keep native method declarations
-keepclasseswithmembernames class * {
    native <methods>;
}

# Groovy runtime scripting
-keep class groovy.** { *; }
-keep class org.codehaus.groovy.** { *; }
-dontwarn groovy.**
-dontwarn org.codehaus.groovy.**

# KryoNet networking
-keep class com.esotericsoftware.** { *; }
-dontwarn com.esotericsoftware.**

# JSON library
-keep class org.json.** { *; }
