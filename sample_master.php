 <?php

// config
$thermostats = array(
    array("Living Room", "192.168.178.43"),
    array("Bedroom", "192.168.178.67")
);
?>
<!doctype html>
<html lang="en">
  <head>
   <meta charset="utf-8">
   <meta name="viewport" content="width=device-width, initial-scale=1">
   <link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/3.4.1/css/bootstrap.min.css" integrity="sha384-HSMxcRTRxnN+Bdg0JdbxYKrThecOKuH5zCYotlSAcp1+c8xmyTe9GYg1l9a69psu" crossorigin="anonymous">
   <script src="https://cdnjs.cloudflare.com/ajax/libs/jquery/3.6.0/jquery.slim.min.js" integrity="sha512-6ORWJX/LrnSjBzwefdNUyLCMTIsGoNP6NftMy2UAm1JBm6PRZCO1d7OHBStWpVFZLO+RerTvqX/Z9mBFfCJZ4A==" crossorigin="anonymous" referrerpolicy="no-referrer"></script>
   <script src="https://cdnjs.cloudflare.com/ajax/libs/bootstrap/5.1.1/js/bootstrap.min.js" integrity="sha512-ewfXo9Gq53e1q1+WDTjaHAGZ8UvCWq0eXONhwDuIoaH8xz2r96uoAYaQCm1oQhnBfRXrvJztNXFsTloJfgbL5Q==" crossorigin="anonymous" referrerpolicy="no-referrer"></script>
   <title>Thermostat Control</title>
  </head>
  <body>
    <div class="container">
      <div class="jumbotron">
        <h1>Thermostat Control</h1>
      </div>

      <div class="row marketing">
<?php
foreach ($thermostats as $thermostat)
{
    // get the info from the thermostat page
  // print it / make a form to change it
  $dom = new DOMDocument;
  $dom->loadHTMLFile("http://" . $thermostat[1]);
  $xpath = new DOMXPath($dom);
  $state = $xpath->query("//dd");
  $values = array();
  foreach ($state as $dd)
  {
    $values[] = trim($dd->nodeValue, "ºC");
  }
?>
        <div class="col-lg-6">
          <h3><?php echo $thermostat[0]; ?></h3>
          <h4>The thermostat is currently <?php if ($values[2] == "0") { echo "<span class='badge'>OFF</span>"; } else { echo "<span class='badge progress-bar-danger'>ON</span>"; } ?></h4>
          <table class="table table-dark table-borderless">
            <tbody>
              <tr>
                <td>Current Temperature: </td><td><?php echo $values[0]; ?>ºC</td>
              </tr>
              <tr>
                <td>Current Relative Humidity: </td><td><?php echo $values[1]; ?></td>
              </tr>
              </tbody>
          </table>
          <h5>The thermostat will turn</h5>
          <form class="form-inline" method="post" action="http://<?php echo $thermostat[1]; ?>">
            <div class="row">
              <div class="col-md-6">
                <div class="input-group">
                  <span class="input-group-addon"><span class="badge progress-bar-danger">ON</span> under</span>
                  <input class="form-control" type="text" name="on" value="<?php echo $values[3]; ?>" />
                  <span class="input-group-addon">ºC</span>
                </div> 
              </div>
              <div class="col-md-6">
               <div class="input-group mb-3">
                 <span class="input-group-addon"><span class="badge">OFF</span> over </span>
                 <input name="off" class="form-control" type="text" value="<?php echo $values[4]; ?>" />
                 <span class="input-group-addon">ºC</span>
                </div>
              </div>
              <div class="col-md-12" style="padding-top: 1em; text-align: center;" >
                <button type="submit" name="save" class="btn btn-lg btn-primary">Apply New Setting</button>
              </div>
            </div>
          </form>
        </div>
<?php

} 
?>
      </div>
    </div>
  </body>
</html>
