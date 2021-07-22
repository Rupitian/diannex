# Information on the Diannex Syntax
The diannex scripting language has a lot of features to offer, some of which might not be as obvious as other. This document will hopefully clear up any confusion regarding the syntax.

## Scenes
To declare a scene, you must use the `scene` keyword followed by an identifier for your scene, you will use this identifier in an Interpreter to run the scene later. You can optionally place your scene in a namespace to help organize your script, and you can nest multiple namespaces if you so wish, just note that if you place your scene in a namespace, you must refer to it in the interpreter with `<namespace_name>.<scene_name>`.

You can also optionally add flags to your scene, flags act as persistant variables that are set the first time you run your scene, and they keep their value so you can do something else the next time you run the scene.

Here's some examples on declaring a scene:
```cs
// Without a namespace, in the interpreter you'd refer to this with just `intro`
scene intro
{
    // Your script code
}

// With a namespace, you'd refer to this with `chapter1.npc_dialogue`
namespace chapter1
{
    scene npc_dialogue
    {
        // Your script code
    }
}

// With nested namespaces, you'd refer to this with `chapter2.utility.test`
namespace chapter2
{
    namespace utility
    {
        scene test
        {
            // Your script code
        }
    }
}

// Here's a scene with a flag
scene flag_test :
    flag(
        // This is the value the flag is set to when the scene first runs
        0,
        // This is an optional name you can give your flag so that you can refer to the value in your game engine
        "runs")
{
    // Your script code, however you can refer to your flag with `$flag`
}
```

## Variables
Diannex supports two types of variables. Local variables, which are scoped to the function, scene or namespace, and global variables, which can be accessed everywhere.

Here's an example of how to define a variable:
```cs
scene intro
{
  local $username = "My User" // Local variable
}
```

## Definitions
Definitions act similarly to a dictionary/map in another language. Their main purpose is for holding strings for elements not necessarily useful for dialogue, for example, translatable text for UI elements. Definitions are NOT used inside of the Diannex files, but in the application itself.

Defining a definition is simple, all you need to do is use the `def` keyword followed by an identifier. Definitions follow all the same rules regarding namespaces as scenes do. However, to be clear, definitions can only store *strings* and nothing else. If you wish to store numbers or other data in a definition, you'll need to put it in a string and convert it to the type you want in your engine.

Here's an example of how to define a definition, taken from our [README](./README.md#basic-language-sample):
```cs
def menu
{
  button_start="Start"
  button_continue="Continue"
  button_quit="Quit"
  // You can still do interpolation in a definition!
  label_welcome="Welcome, ${getPlayerName()}!"
}
```

## Functions
You can define functions in your diannex script so that you can reuse pieces of code in your scenes without interoperating with your engine code. Functions can be declared just by using the `func` keyword followed by an identifier. Functions can also take in arguments and return a value. Functions, like scenes, can also take flags, they are defined the same way and used the same way as well. One thing to note about functions however is that when you're calling them in your scenes, you must make note of the namespace the function is in, if it's in the same namespace as your scene, you can just use the function name, but if it's in another namespace, you must fully qualify the name using `<namespace>.<function>`.

Here's some examples on defining a function:
```cs
// Outside of a namespace, you can just refer to this function by name, even if your scene is in another namespace.
func example()
{
    // Your function code
}

// Inside of a namespace, if your scene is also in the namespace, you can refer to this function by just `example`, otherwise you must qualify it with `test.example`
namespace test
{
    // Your function can also take arguments!
    func example(a)
    {
        // Your function code, however you can refer to your argument with `$a`
    }
}

// Here's an example on actually calling your function
scene test
{
    // Call the example function that's not in a namespace
    example // Parenthesis are optional

    // Call the example function that's in a namespace, and pass in 20 as an argument
    test.example(20)
}
```

## Control Statements
The diannex scripting language has various control statements that you may not see in other programming languages. These are `choose`, `choice`, `repeat`, and `sequence`. We also of course have the trademark control statements of `if`, `else`, `for`, `while`, and `switch`. I'll refrain from going over the trademark statements and instead focus a little more on each of our unique statements.

### Choose
The `choose` statement allows a user to randomly pick between multiple options. You can optionally specify a weight for options in a percentage. By default each option has an equal chance of being choosen.

Here's an example of using `choose` taken from our example script in the [README](./README.md#basic-language-sample):
```cs
func example(a)
{
    // This statement will randomly pick between both lines of dialogue
    choose
    {
        // These statements are using what's called Interpolation to display the value of the variable in the dialogue, more on that later.
        "This is an example function, being passed ${$a}"
        "This is an example function (but an alternate line with 50% chance), being passed ${$a}"
    }
}
```

### Choice
While the `choice` statement might sound similar to the `choose` statement, there are huge differences between them! The `choice` statement is used to allow the user to select an option, which then allows you to run different parts of your script based on their choice. However, much like the `choose` statement, you can specify a chance for either option as a percentage. However, unlike `choose`, this value is the chance of the option appearing. So having an option have a 50% chance would mean the option has a 50% chance of appearing to the user to be selected.

Here's an example of using `choice`, once again taken from our example script in the [README](./README.md#basic-language-sample):
```cs
scene intro
{
    /**
     * This will display the prompt "Is this a question?" to the user.
     * As well as the choices "Yes" and "No".
     *
     * No chance has been defined for either choice to that means both
     * choices are guaranteed to appear to the user.
     */
    choice "Is this a question?"
    {
        "Yes"
        {
            // If the user picks "Yes" this block of code will run.
            "That is correct."
            awardPoints 1
        }
        "No"
        {
            // If the user picks "No" this block of code will run.
            "Hm... I don't believe that's correct."
            deductPoints true, 5
        }
    }
}
```

### Repeat
The `repeat` statement is much like a condensed `for` loop. Much like what the name implies, the `repeat` statement will run the code under it as many times as specified. This is mainly useful for when you wish to have a loop but don't care for the specific iteration of the loop.

Here's another example pulled from our [README](./README.md#basic-language-sample) showcasing the `repeat` statement:
```cs
scene intro
{
    //! This line is repeated 5 times
    repeat (5)
        "The same thing, over and over..."
}
```

### Sequence
And finally, we have the `sequence` statement. The `sequence` statement works similarly to a `switch` statement. The main caveat however, is the expression passed gets incremented after every run. The expression must be a variable or an index into an array for it to work. Additionally, you can use `continue` and `break` to use the `sequence` more like a loop, where `continue` will move ahead to the next iteration, and `break` will leave the sequence altogether.

Here's an example on using the `sequence` statement:
```cs
sequence $var // optional parentheses
{
    0: "This is the initial line"
    1:
    {
        "This is the second line"
    }, // optional comma
    2..3: "This runs the third and fourth times the code is executed"
    100: "This runs the fifth time, and every time after"
}

/**
 * Here's a sequence that's using an index into an array.
 *
 * In this specific example, the value in the array `$arr`
 * at index `0` will be increment after every run.
 */
sequence $arr[0]
{
    0: "This runs the first time"
    1: "This runs the second time"
    2: "This runs the third time, and every time after"
}
```

## Diannex Gotchas
There are also a few things they may catch you by surprise if you're not aware of them. Mainly the `char` method, optional parenthesis/braces, and `interpolation`.

## Char
The `char` method is an implied method that allows your script to change the character that's speaking. This is useful for selecting the specific sprite of your character or the name of a character if your game has dialogue boxes. It's an implied method because it's not actually defined in diannex. The script expects your engine to define the method if you use `char`. Additionally, since `char` is expected to be used a lot, there's some syntax sugar for it. This comes in the form of
```cs
<char_name>: "Dialogue"
```
You can see it being used in the sample program in the [README](./README.md#basic-language-sample). This is equivalent to running the following:
```cs
char "<char_name>"
"Dialogue"
```

## Optional Parenthesis/Braces
In a lot of parts in your diannex script you can forego parenthesis and braces.
<br/>
TODO: Explain more

## Interpolation
Diannex supports string interpolation out of the box, however unlike other languages, this doesn't require using a prefix on the string or a custom string character, just use `${<expression>}` anywhere in your string to replace the block with the result of the expression. If you want to use your own method of interpolation, or if you don't want to use interpolation at all, you can disable Diannex's interpolation by setting the `interpolation_enabled` flag to `false` in your diannex project file.
