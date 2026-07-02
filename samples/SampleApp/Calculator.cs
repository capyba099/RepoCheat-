namespace SampleApp;

public class Calculator
{
    public int Add(int a, int b)
    {
        return a + b;
    }

    public int Multiply(int a, int b)
    {
        int result = 0;
        for (int i = 0; i < b; i++)
        {
            result = result + a;
        }
        return result;
    }

    public static string Greet(string name)
    {
        if (name == null)
        {
            return "Hello, stranger!";
        }
        return "Hello, " + name + "!";
    }
}

public class Point
{
    public int X;
    public int Y;

    public Point(int x, int y)
    {
        X = x;
        Y = y;
    }

    public int DistanceSquared()
    {
        return X * X + Y * Y;
    }
}
